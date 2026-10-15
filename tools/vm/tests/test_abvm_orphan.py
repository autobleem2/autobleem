"""abvm's sandbox start/stop against a faked guest: no orphan launcher, no false 'ready', the port chosen by what the
guest listens on (python -m unittest discover tools/vm/tests)"""
import builtins
import os
import re
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import abvm  # noqa: E402


class FakeGuest:
    """a guest with processes {pid: (root, port or None)}; the launcher binds its port only when it is free"""

    def __init__(self):
        self.procs = {}
        self.next_pid = 4000
        self.silent = False  # a launcher that never opens its port
        self.killed = []

    def listeners(self):
        return {port: pid for pid, (_, port) in self.procs.items() if port}

    def spawn(self, root, port, listens=True):
        self.next_pid += 1
        self.procs[self.next_pid] = (root, port if listens else None)
        return self.next_pid

    def __call__(self, cmd, check=True):
        if cmd == 'ss -ltn':
            return 'State Recv-Q Send-Q Local Address:Port Peer Address:Port\n' + ''.join(
                f'LISTEN 0 128 127.0.0.1:{p} 0.0.0.0:*\n' for p in self.listeners())
        if cmd.startswith('for p in $(pidof'):
            return ''.join(f'{pid} ./autobleem-gui {root} \n' for pid, (root, _) in self.procs.items())
        m = re.match(r'cat /proc/(\d+)/cmdline', cmd)
        if m:
            p = self.procs.get(int(m.group(1)))
            return f'./autobleem-gui {p[0]} ' if p else ''
        if cmd.startswith('set -e; mountpoint'):
            port = int(re.search(r'AB_DEBUG_PORT=(\d+)', cmd).group(1))
            root = re.search(r'./autobleem-gui (\S+) >', cmd).group(1)
            bound = port not in self.listeners() and not self.silent
            pid = self.spawn(root, port, listens=bound) if not (port in self.listeners()) else None
            if pid is None:  # the launcher cannot bind and exits at once
                self.next_pid += 1
                pid = self.next_pid
            out = f'pid {pid}\n'
            owner = self.listeners().get(port)
            if owner:  # the guest's ss shows the listener, whoever's it is
                out += f'listen LISTEN 0 128 127.0.0.1:{port} 0.0.0.0:* users:(("autobleem-gui",pid={owner},fd=9))\n'
            return out
        m = re.match(r'kill -TERM ([\d ]+) ', cmd)
        if m:
            for p in map(int, m.group(1).split()):
                self.killed.append(p)
                self.procs.pop(p, None)
        return ''


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.host = self.tmp.name
        self.guest = FakeGuest()

        class Driver:
            def __init__(self, port):
                pass

            def wait_screen(self, *a):
                pass

            def close(self):
                pass

        fake_socket = mock.MagicMock()
        self.patches = [mock.patch.object(abvm, 'SB_HOST', self.host),
                        mock.patch.object(abvm, 'guest_run', self.guest),
                        mock.patch.object(abvm, 'sb_open_modes', lambda p: None),
                        mock.patch.object(abvm, 'sb_open_tree', lambda n, acl=False: None),
                        mock.patch.object(abvm, 'sb_share_group', lambda strict=False: None),
                        mock.patch.object(abvm, 'sb_forward', lambda port: 0),
                        mock.patch.object(abvm, 'sb_stop_forward', lambda st: None),
                        mock.patch.object(abvm, 'import_ab_drive', lambda: mock.Mock(Driver=Driver)),
                        mock.patch.object(abvm.socket, 'socket', return_value=fake_socket),
                        mock.patch('builtins.print')]
        for p in self.patches:
            p.start()
        for n in ('sb1', 'sb2'):
            os.makedirs(os.path.join(self.host, n))

    def tearDown(self):
        for p in self.patches:
            p.stop()
        self.tmp.cleanup()

    def root(self, name):
        return f'{abvm.SB_GUEST}/{name}'


class Start(Base):
    def test_port_is_picked_by_what_the_guest_listens_on(self):
        self.guest.spawn(self.root('sb2'), 6910)  # an orphan: no state file
        with mock.patch.object(abvm, 'sb_running_names', return_value=[]):
            abvm.sb_start('sb1')
        self.assertEqual(abvm.sb_state('sb1')['port'], 6911)

    def test_two_sandbox_collision_orphan_then_second_start(self):
        self.guest.silent = True  # sb2's launcher runs but never opens its port
        with self.assertRaises(abvm.Fail):
            abvm.sb_start('sb2')
        self.assertEqual(self.guest.procs, {})  # killed, not left as an orphan
        self.assertEqual(abvm.sb_state('sb2'), {})
        self.guest.silent = False
        abvm.sb_start('sb1')
        st = abvm.sb_state('sb1')
        self.assertEqual(self.guest.listeners(), {st['port']: st['pid']})
        self.assertEqual(self.guest.procs[st['pid']][0], self.root('sb1'))

    def test_collision_with_an_existing_orphan_never_reports_ready(self):
        orphan = self.guest.spawn(self.root('sb2'), 6910)
        # even when the port choice was fooled (the orphan was not seen), the other's listener is not "ready"
        with mock.patch.object(abvm, 'sb_guest_ports', return_value=set()):
            with self.assertRaises(abvm.Fail):
                abvm.sb_start('sb1')
        self.assertEqual(abvm.sb_state('sb1'), {})
        self.assertIn(orphan, self.guest.procs)  # the orphan is not sb1's to kill

    def test_ready_needs_the_listener_to_be_this_launchers(self):
        out = 'pid 4001\nlisten LISTEN 0 128 127.0.0.1:6910 0.0.0.0:* users:(("autobleem-gui",pid=4999,fd=9))\n'
        self.assertIsNone(abvm.sb_port_owner('sb1', 4001, out))
        self.guest.procs[4001] = (self.root('sb1'), 6910)
        self.assertIsNone(abvm.sb_port_owner('sb1', 4001, out))  # owner 4999 is not a launcher of sb1
        self.assertEqual(abvm.sb_port_owner('sb1', 4001, out.replace('4999', '4001')), 4001)
        self.assertIsNone(abvm.sb_port_owner('sb1', 4001, 'pid 4001\n'))

    def test_failed_start_keeps_the_log_and_makes_its_folder(self):
        self.guest.silent = True
        log = os.path.join(self.host, 'sb1', 'System', 'Logs', 'abvm-out.txt')

        def write_log(cmd, check=True):
            if cmd.startswith('set -e; mountpoint'):
                self.assertTrue(os.path.isdir(os.path.dirname(log)))  # the redirect needs it
                with open(log, 'w') as f:
                    f.write('boom\n')
            return self.guest(cmd, check)

        with mock.patch.object(abvm, 'guest_run', write_log):
            with self.assertRaises(abvm.Fail):
                abvm.sb_start('sb1')
        with open(log) as f:
            self.assertEqual(f.read(), 'boom\n')


class OrphanCommands(Base):
    def test_list_and_stop_see_and_kill_an_orphan(self):
        pid = self.guest.spawn(self.root('sb2'), 6910)
        self.assertEqual(abvm.sb_orphans(), {'sb2': [pid]})
        with mock.patch.object(abvm, 'lease_op', return_value='free'):
            abvm.sb_list()
        text = '\n'.join(c[0][0] for c in builtins.print.call_args_list)  # print is patched in setUp
        self.assertRegex(text, rf'sb2 +orphan \(pid {pid}\)')
        abvm.sb_stop('sb2')
        self.assertIn(pid, self.guest.killed)
        self.assertEqual(self.guest.procs, {})

    def test_a_running_sandbox_is_not_an_orphan(self):
        abvm.sb_start('sb1')
        self.assertEqual(abvm.sb_orphans(), {})


if __name__ == '__main__':
    unittest.main()
