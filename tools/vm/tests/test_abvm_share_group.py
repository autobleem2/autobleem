"""abvm's share group and the probe step: no VM (python -m unittest discover tools/vm/tests)"""
import os
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import abvm  # noqa: E402


class ShareGid(unittest.TestCase):
    def test_the_gid_is_read_from_the_host_group(self):
        with mock.patch('grp.getgrnam', return_value=SimpleNamespace(gr_gid=64055)) as g:
            self.assertEqual(abvm.sb_share_gid(), 64055)
        g.assert_called_with('libvirt-qemu')

    def test_without_the_group_a_folder_qemu_made_gives_it(self):
        with tempfile.TemporaryDirectory() as host:
            os.makedirs(os.path.join(host, 'qemu-made'))
            theirs = SimpleNamespace(st_uid=os.getuid() + 1, st_gid=777)
            with mock.patch.object(abvm, 'SB_HOST', host), mock.patch('grp.getgrnam', side_effect=KeyError), \
                    mock.patch.object(abvm.os, 'stat', return_value=theirs):
                self.assertEqual(abvm.sb_share_gid(), 777)

    def test_unknown_gid_is_none(self):
        with tempfile.TemporaryDirectory() as host:
            with mock.patch.object(abvm, 'SB_HOST', host), mock.patch('grp.getgrnam', side_effect=KeyError):
                self.assertIsNone(abvm.sb_share_gid())


class GroupCommands(unittest.TestCase):
    def test_script_uses_the_gid_and_only_changes_what_is_missing(self):
        script = abvm.sb_group_script(64055)
        self.assertIn('getent group 64055', script)
        self.assertIn('[ -n "$g" ] || { sudo groupadd -g 64055 abvmshare;', script)  # only when no group has the gid
        self.assertIn('grep -qx 64055 || sudo usermod -aG "$g" autobleem', script)  # only when not yet a member

    def test_group_is_set_up_from_the_hosts_gid_in_the_guest(self):
        calls = []
        with mock.patch.object(abvm, 'sb_share_gid', return_value=4242), \
                mock.patch.object(abvm, 'guest_run', lambda c, check=True: calls.append((c, check)) or ''):
            self.assertEqual(abvm.sb_share_group(), 4242)
        self.assertEqual(len(calls), 1)
        self.assertIn('groupadd -g 4242', calls[0][0])
        self.assertFalse(calls[0][1])  # at start a failure is not fatal: sb_open_tree is the net

    def test_setup_is_strict_and_unknown_gid_is_a_failure_there_only(self):
        with mock.patch.object(abvm, 'sb_share_gid', return_value=None), \
                mock.patch.object(abvm, 'guest_run', lambda c, check=True: self.fail('no gid, no guest call')):
            self.assertIsNone(abvm.sb_share_group())
            with self.assertRaises(abvm.Fail):
                abvm.sb_share_group(strict=True)

    def test_sandbox_start_checks_the_group_before_the_launcher(self):
        import inspect
        src = inspect.getsource(abvm.sb_start)
        self.assertLess(src.index('sb_share_group()'), src.index('sb_open_tree(name, acl=True)'))
        self.assertIn('sb_share_group(strict=True)', inspect.getsource(abvm.sb_setup))


class Probe(unittest.TestCase):
    def test_escapes_are_refused(self):
        for bad in ('', '..', '../x', 'a/../../x', '/etc/passwd', '\\x', 'C:/x', 'a\\..\\b'):
            with self.assertRaises(abvm.Fail, msg=bad):
                abvm.sb_probe_path('sb1', bad)
        self.assertEqual(abvm.sb_probe_path('sb1', 'System/Updates'), abvm.SB_GUEST + '/sb1/System/Updates')
        self.assertEqual(abvm.sb_probe_path('sb1', 'a..b/c'), abvm.SB_GUEST + '/sb1/a..b/c')  # not a component

    def test_probe_is_read_only_and_shows_user_owner_and_parent(self):
        calls = []
        with mock.patch.object(abvm, 'guest_run', lambda c, check=True: calls.append(c) or 'uid=1000(autobleem)\n'):
            out = abvm.sb_probe('sb1', 'System/Updates')
        cmd = calls[0]
        self.assertTrue(cmd.startswith('id autobleem; stat -c'))
        self.assertIn(abvm.SB_GUEST + '/sb1/System/Updates', cmd)
        self.assertIn(abvm.SB_GUEST + '/sb1/System;', cmd)
        self.assertIn('ls -la', cmd)
        for word in ('rm ', 'chmod', 'touch', 'mkdir', 'sudo', '>'):
            self.assertNotIn(word, cmd)
        self.assertIn('uid=1000(autobleem)', out)

    def test_split_keeps_order_and_rejects_before_anything_runs(self):
        self.assertEqual(abvm.sb_split_probes('sb1', '@1 tap a; probe System/Updates; wait_idle 5; shot a.png'),
                         [('drive', '@1 tap a'), ('probe', 'System/Updates'), ('drive', 'wait_idle 5; shot a.png')])
        with self.assertRaises(abvm.Fail):
            abvm.sb_split_probes('sb1', 'wait_idle 5; probe ../x')

    def test_probe_never_reaches_the_driver(self):
        with tempfile.TemporaryDirectory() as host:
            os.makedirs(os.path.join(host, 'sb1'))
            sent, probed = [], []

            class FakeDriver:
                def run(self, script):
                    sent.append(script)
                    return ['ok']

                def close(self):
                    pass

            with mock.patch.object(abvm, 'SB_HOST', host), mock.patch.object(abvm, 'sb_driver', lambda n, **kw: FakeDriver()), \
                    mock.patch.object(abvm, 'sb_open_modes', lambda p: None), \
                    mock.patch.object(abvm, 'guest_run', lambda c, check=True: probed.append(c) or 'x'):
                abvm.sb_drive('sb1', 'wait_idle 1; probe System; shot a.png', host)
                self.assertEqual(sent, ['wait_idle 1', 'shot a.png'])
                self.assertEqual(len(probed), 1)
                sent.clear()
                with self.assertRaises(abvm.Fail):
                    abvm.sb_drive('sb1', 'wait_idle 1; probe /etc', host)
                self.assertEqual(sent, [])


if __name__ == '__main__':
    unittest.main()
