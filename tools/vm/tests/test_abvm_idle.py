"""abvm vm idle-check / ensure-running and the last-use file (a faked virsh and guest: no VM is touched)"""
import contextlib
import io
import json
import os
import sys
import tempfile
import time
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import abvm  # noqa: E402

LAUNCHER = '/media/autobleem/Autobleem/bin/autobleem/autobleem-gui /media/autobleem\n'


class Idle(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.state = 'running'          # what `virsh domstate` says
        self.cmdlines = LAUNCHER        # the guest's autobleem-gui command lines (the stick's launcher is always there)
        self.guest_up = True            # does ssh to the guest answer
        self.virsh = []                 # the virsh verbs run
        self.sleeps = []
        self.patches = [
            mock.patch.dict(os.environ, {'HOME': self.tmp.name}),
            mock.patch.object(abvm, 'SB_HOST', os.path.join(self.tmp.name, 'sandboxes')),
            mock.patch.object(abvm, 'LOCAL', True),
            mock.patch.object(abvm, 'WHO', 'alice'),
            mock.patch.object(abvm, 'LEASE_KEY', 'vm'),
            mock.patch.object(abvm, '_guest', 'autobleem@10.0.0.9'),
            mock.patch.object(abvm, 'host_run', side_effect=self.host_run),
            mock.patch.object(abvm.time, 'sleep', side_effect=self.sleeps.append),
        ]
        for p in self.patches:
            p.start()

    def tearDown(self):
        for p in self.patches:
            p.stop()
        self.tmp.cleanup()

    def host_run(self, command, check=True, full=False):
        if 'domstate' in command:
            return self.state + '\n'
        if ' suspend ' in command or ' resume ' in command:
            verb = 'suspend' if ' suspend ' in command else 'resume'
            self.virsh.append(verb)
            self.state = 'paused' if verb == 'suspend' else 'running'
            return ''
        if 'pidof autobleem-gui' in command:
            if not self.guest_up:
                return mock.Mock(stdout='', stderr='ssh: timeout', returncode=255)
            return mock.Mock(stdout=self.cmdlines, stderr='', returncode=0)
        if command.endswith(' true'):  # the guest's ssh probe
            return mock.Mock(stdout='', stderr='', returncode=0 if self.guest_up else 255)
        raise AssertionError(command)

    def main(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = abvm.main(['--local', '--who', 'alice'] + list(argv))
        return code, out.getvalue(), err.getvalue()

    def used(self, minutes_ago):
        os.makedirs(os.path.dirname(abvm.state_path('last-use')), exist_ok=True)
        with open(abvm.state_path('last-use'), 'w') as f:
            f.write(str(int(time.time() - minutes_ago * 60)))

    def lease(self, key):
        saved = abvm.LEASE_KEY
        abvm.LEASE_KEY = key
        try:
            abvm.lease_op('take', 'bob', 'a task', 30)
        finally:
            abvm.LEASE_KEY = saved

    def check(self, *flags):
        code, out, err = self.main('vm', 'idle-check', *flags)
        self.assertEqual(code, 0, err)
        return out.strip()

    # --- suspends only when idle by every condition

    def test_idle_suspends_and_records_it(self):
        self.used(30)
        self.assertEqual(self.check(), 'suspended: idle for 20 min')
        self.assertEqual(self.virsh, ['suspend'])
        with open(abvm.state_path('vm-suspended.json')) as f:
            self.assertEqual(json.load(f)['how'], 'virsh suspend')

    def test_minutes_option(self):
        self.used(30)
        self.assertTrue(self.check('--minutes', '45').startswith('not idle: used 30 min ago (less than 45)'))
        self.assertEqual(self.virsh, [])
        self.assertEqual(self.check('--minutes', '10'), 'suspended: idle for 10 min')

    def test_a_pool_slot_lease_blocks(self):
        self.used(30)
        self.lease('sb-sb3')
        self.assertIn('not idle: sandbox slot sb3 has a lease', self.check())
        self.assertEqual(self.virsh, [])

    def test_the_vm_lease_blocks(self):
        self.used(30)
        self.lease('vm')
        self.assertIn('not idle: the stick launcher is in use', self.check())
        self.assertEqual(self.virsh, [])

    def test_a_sandbox_launcher_blocks(self):
        self.used(30)
        self.cmdlines += '/mnt/abvm/sb2/Autobleem/bin/autobleem/autobleem-gui /mnt/abvm/sb2 \n'
        self.assertIn('not idle: a launcher runs in the guest: sb2', self.check())
        self.assertEqual(self.virsh, [])

    def test_an_orphan_blocks(self):
        # no lease, no state file - only the guest's process list shows it, and under a name outside the pool
        self.used(30)
        self.cmdlines += '/mnt/abvm/old-thing/Autobleem/bin/autobleem/autobleem-gui /mnt/abvm/old-thing \n'
        self.assertIn('old-thing', self.check())
        self.assertEqual(self.virsh, [])

    def test_recent_use_blocks(self):
        self.used(5)
        self.assertIn('not idle: used 5 min ago', self.check())
        self.assertEqual(self.virsh, [])

    def test_no_last_use_record_starts_the_clock_and_blocks(self):
        self.assertIn('not idle: no last-use record', self.check())
        self.assertEqual(self.virsh, [])
        self.assertIsNotNone(abvm.last_use())

    def test_a_guest_that_does_not_answer_is_left_alone(self):
        self.used(30)
        self.guest_up = False
        code, out, err = self.main('vm', 'idle-check')
        self.assertEqual(code, 1)
        self.assertIn('cannot look into the guest', err)
        self.assertEqual(self.virsh, [])

    def test_an_expired_lease_does_not_block(self):
        self.used(30)
        self.lease('sb-sb1')
        path = abvm.state_path('lock-sb-sb1.json')
        with open(path) as f:
            lease = json.load(f)
        lease['until'] = time.time() - 1
        with open(path, 'w') as f:
            json.dump(lease, f)
        self.assertEqual(self.check(), 'suspended: idle for 20 min')

    def test_the_stick_launcher_alone_does_not_block(self):
        self.used(30)
        self.assertEqual(self.cmdlines, LAUNCHER)
        self.assertTrue(self.check().startswith('suspended'))

    def test_already_suspended_and_not_running(self):
        self.state = 'paused'
        self.assertEqual(self.check(), 'already suspended')
        self.state = 'shut off'
        self.assertEqual(self.check(), 'not running: shut off')
        self.assertEqual(self.virsh, [])

    def test_dry_run_changes_nothing(self):
        self.used(30)
        self.assertEqual(self.check('--dry-run'), 'would suspend: idle for 20 min')
        self.assertEqual(self.virsh, [])
        self.assertFalse(os.path.exists(abvm.state_path('vm-suspended.json')))

    def test_usage_errors_are_exit_2(self):
        self.assertEqual(self.main('vm', 'idle-check', '--minutes', 'x')[0], 2)
        self.assertEqual(self.main('vm', 'idle-check', '--bogus')[0], 2)
        self.assertEqual(self.main('vm', 'nope')[0], 2)

    # --- resume before use

    def test_a_command_that_needs_the_guest_resumes_a_suspended_vm_first(self):
        self.state = 'paused'
        order = []
        with mock.patch.object(abvm, 'status', lambda: order.append(('status', self.state)) or 0):
            code, out, err = self.main('status')
        self.assertEqual(code, 0)
        self.assertEqual(self.virsh, ['resume'])
        self.assertEqual(order, [('status', 'running')])          # resumed BEFORE the command ran
        self.assertIn('resuming the VM...', err)

    def test_every_guest_command_is_covered(self):
        for cmd in ('status', 'install', 'restore', 'restart', 'update', 'remount', 'clip', 'pad', 'run', 'drive',
                    'padsim-install', 'guest', 'sandbox'):
            self.assertIn(cmd, abvm.GUEST_COMMANDS)
        self.state = 'paused'
        with mock.patch.object(abvm, 'sandbox_command', lambda a, o: None):
            self.main('sandbox', 'take-free', 'task')
        self.assertEqual(self.virsh, ['resume'])

    def test_a_running_vm_is_not_touched(self):
        with mock.patch.object(abvm, 'status', lambda: 0):
            self.main('status')
        self.assertEqual(self.virsh, [])

    def test_resume_waits_for_the_guest(self):
        self.state = 'paused'
        answers = iter([False, False, True])
        with mock.patch.object(abvm, 'vm_guest_answers', lambda: next(answers)):
            code, out, err = self.main('vm', 'ensure-running')
        self.assertEqual(code, 0)
        self.assertTrue(out.startswith('resumed (guest answered after'))
        self.assertEqual(len(self.sleeps), 2)

    def test_resume_gives_up_after_60_s(self):
        self.state = 'paused'
        self.guest_up = False
        clock = iter(range(0, 1000, 20))
        with mock.patch.object(abvm.time, 'time', lambda: next(clock)):
            code, out, err = self.main('vm', 'ensure-running')
        self.assertEqual(code, 1)
        self.assertIn('did not answer ssh in 60 s', err)

    def test_a_shut_off_vm_is_an_error_not_started(self):
        self.state = 'shut off'
        code, out, err = self.main('vm', 'ensure-running')
        self.assertEqual(code, 1)
        self.assertEqual(self.virsh, [])

    def test_resume_clears_the_record_and_restarts_the_idle_clock(self):
        self.used(30)
        self.check()
        self.assertTrue(os.path.exists(abvm.state_path('vm-suspended.json')))
        self.main('vm', 'ensure-running')
        self.assertFalse(os.path.exists(abvm.state_path('vm-suspended.json')))
        self.assertLess(time.time() - abvm.last_use(), 5)
        self.assertIn('not idle', self.check())                   # so the next tick does not suspend it again

    # --- the last-use file

    def test_commands_that_take_a_lease_or_drive_record_the_use(self):
        for argv in (('lock', 'take', 'task'), ('sandbox', 'take', 'sb1', 'task'), ('sandbox', 'take-free', 'task'),
                     ('sandbox', 'drive', 'sb1', '@1 tap a'), ('guest', 'ls'), ('drive', 'tap a')):
            self.used(100)
            with mock.patch.object(abvm, 'sandbox_command', lambda a, o: None), \
                    mock.patch.object(abvm, 'lease', lambda *a: 'ok'), \
                    mock.patch.object(abvm, 'lease_op', lambda *a, **k: 'taken'), \
                    mock.patch.object(abvm, 'guest_run', lambda *a, **k: ''), \
                    mock.patch.object(abvm, 'drive', lambda s: None):
                self.main(*argv)
            self.assertLess(time.time() - abvm.last_use(), 5, argv)

    def test_reading_commands_do_not_record_a_use(self):
        for argv in (('lock', 'status'), ('sandbox', 'list'), ('sandbox', 'logs', 'sb1'), ('vm', 'status')):
            self.used(100)
            with mock.patch.object(abvm, 'sandbox_command', lambda a, o: None), \
                    mock.patch.object(abvm, 'lease', lambda *a: 'free'):
                self.main(*argv)
            self.assertGreater(time.time() - abvm.last_use(), 90, argv)

    def test_from_a_pc_the_test_machines_copy_does_the_work(self):
        calls = []
        with mock.patch.object(abvm, 'LOCAL', False), \
                mock.patch.object(abvm, 'remote_tool', lambda args, partial=False: calls.append(args) or ''):
            abvm.vm_prepare('guest', ['ls'])
            abvm.vm_prepare('sandbox', ['take-free', 't'])   # delegates whole: its remote main prepares
            abvm.vm_prepare('lock', ['take', 't'])            # remote `lock take` records the use
            abvm.vm_prepare('shot', ['a.png'])
            self.assertEqual(abvm.main(['vm', 'idle-check', '--dry-run']), 0)
        self.assertEqual(calls, [['vm', 'ensure-running', '--touch'], ['vm', 'idle-check', '--dry-run']])


if __name__ == '__main__':
    unittest.main()
