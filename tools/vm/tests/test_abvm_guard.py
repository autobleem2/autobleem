"""abvm's CPU guard (two sandboxes at most) and the stick remount: pure logic, touches no VM
(python -m unittest discover tools/vm/tests)"""
import importlib
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import abvm  # noqa: E402


def reload_with(slots):
    env = dict(os.environ)
    env.pop('ABVM_SANDBOX_SLOTS', None)
    if slots is not None:
        env['ABVM_SANDBOX_SLOTS'] = slots
    with mock.patch.dict(os.environ, env, clear=True):
        return importlib.reload(abvm).SB_SLOTS


class Ceiling(unittest.TestCase):
    def tearDown(self):
        reload_with(None)

    def test_default_and_env_can_only_lower(self):
        self.assertEqual(reload_with(None), 2)
        self.assertEqual(reload_with('1'), 1)
        self.assertEqual(reload_with('2'), 2)
        self.assertEqual(reload_with('3'), 2)
        self.assertEqual(reload_with('99'), 2)
        self.assertEqual(reload_with('0'), 1)
        self.assertEqual(reload_with('x'), 2)


class Counting(unittest.TestCase):
    def test_names_from_the_guests_process_list(self):
        text = ('./autobleem-gui /mnt/abvm/ela-one \n'
                '/media/autobleem/Autobleem/bin/autobleem/autobleem-gui /media/autobleem \n'
                './autobleem-gui /mnt/abvm/george-2\n'
                'vim /mnt/abvm/notes\n')
        self.assertEqual(abvm.sb_names_in_cmdlines(text), {'ela-one', 'george-2'})

    def test_third_start_is_refused_naming_the_running_ones(self):
        with tempfile.TemporaryDirectory() as host, \
                mock.patch.object(abvm, 'SB_HOST', host), \
                mock.patch.object(abvm, 'SB_SLOTS', 2), \
                mock.patch.object(abvm, 'guest_run', return_value='./autobleem-gui /mnt/abvm/a \n'
                                                                  './autobleem-gui /mnt/abvm/b \n'), \
                mock.patch.object(abvm, 'sb_new') as new:
            os.makedirs(os.path.join(host, 'c'))
            with self.assertRaises(abvm.Busy) as cm:
                abvm.sb_start('c')
            self.assertIn('a, b', str(cm.exception))
            self.assertIn('sandbox stop', str(cm.exception))
            new.assert_not_called()

    def test_the_starting_one_does_not_count_against_itself(self):
        with tempfile.TemporaryDirectory() as host, \
                mock.patch.object(abvm, 'SB_HOST', host), \
                mock.patch.object(abvm, 'guest_run', return_value='./autobleem-gui /mnt/abvm/a \n'):
            os.makedirs(os.path.join(host, 'a'))
            self.assertEqual(abvm.sb_running_names(exclude='a'), [])
            self.assertEqual(abvm.sb_running_names(), ['a'])


LSBLK = ('sda  \n'
         'sda1 AAAA-1111 BOOT 11111111-01 \n'
         'sda3 1234-ABCD AUTO\\x20BLEEM 11111111-03 data\n')


class Remount(unittest.TestCase):
    def test_source_of_the_mount_unit(self):
        self.assertEqual(abvm.stick_source('/dev/disk/by-uuid/1234-ABCD'), ('uuid', '1234-ABCD'))
        self.assertEqual(abvm.stick_source('/dev/disk/by-label/AUTO\\x20BLEEM'), ('label', 'AUTO BLEEM'))
        self.assertEqual(abvm.stick_source('/dev/sdb3'), ('dev', '/dev/sdb3'))
        self.assertEqual(abvm.stick_source(''), (None, ''))

    def test_partition_found_by_uuid_label_never_hard_coded(self):
        self.assertEqual(abvm.find_stick_partition(LSBLK, 'uuid', '1234-ABCD'), '/dev/sda3')
        self.assertEqual(abvm.find_stick_partition(LSBLK, 'label', 'AUTO BLEEM'), '/dev/sda3')
        self.assertEqual(abvm.find_stick_partition(LSBLK, 'partuuid', '11111111-03'), '/dev/sda3')
        self.assertIsNone(abvm.find_stick_partition(LSBLK, 'uuid', 'FFFF-0000'))
        self.assertEqual(abvm.find_stick_partition(LSBLK, 'dev', '/dev/sda1'), '/dev/sda1')

    def run_remount(self, states, apply=True):
        calls = []
        shown = iter(states)

        def guest(cmd, check=True):
            calls.append(cmd)
            if cmd.startswith('systemctl show'):
                return next(shown)
            if cmd.startswith('lsblk'):
                return LSBLK
            return ''
        with mock.patch.object(abvm, 'guest_run', side_effect=guest):
            return abvm.remount(apply), calls

    FAILED = 'ActiveState=failed\nWhat=/dev/disk/by-uuid/1234-ABCD\n'

    def test_active_mount_changes_nothing(self):
        out, calls = self.run_remount(['ActiveState=active\nWhat=/dev/disk/by-uuid/1234-ABCD\n'])
        self.assertEqual(len(out), 1)
        self.assertFalse(any('sudo' in c for c in calls))

    def test_failed_mount_triggers_that_partition_then_mounts_and_starts_the_launcher(self):
        out, calls = self.run_remount([self.FAILED, 'ActiveState=active\n'])
        sudo = [c for c in calls if 'sudo' in c]
        self.assertIn('udevadm trigger --name-match=/dev/sda3', sudo[0])
        self.assertIn('start media-autobleem.mount', sudo[1])
        self.assertIn('start autobleem.service', sudo[2])
        self.assertFalse(any(w in c for c in calls for w in ('mkfs', 'fsck', 'format')))

    def test_status_only_reports(self):
        out, calls = self.run_remount([self.FAILED], apply=False)
        self.assertIn('/dev/sda3', ' '.join(out))
        self.assertFalse(any('sudo' in c for c in calls))

    def test_still_failing_after_trigger_stops_without_starting_the_launcher(self):
        with self.assertRaises(abvm.Fail):
            self.run_remount([self.FAILED, 'ActiveState=failed\n'])


class Release(unittest.TestCase):
    """a release stops a launcher its holder forgot to stop; never one someone else holds"""

    def release(self, holder, running, who='ela'):
        with mock.patch.object(abvm, 'WHO', who), \
                mock.patch.object(abvm, 'sb_running', return_value={'pid': 7} if running else {}), \
                mock.patch.object(abvm, 'sb_stop') as stop, \
                mock.patch.object(abvm, 'lease_op', side_effect=lambda op, *a: holder if op == 'show' else 'ok'):
            abvm.sb_release('a')
        return stop

    def test_own_lease_with_a_running_launcher_stops_it(self):
        self.release('held by ela (walk) since 21:00, 5 min left', True).assert_called_once_with('a')

    def test_free_lease_with_a_left_launcher_stops_it(self):
        self.release('free', True).assert_called_once_with('a')

    def test_someone_elses_sandbox_is_left_alone(self):
        self.release('held by tola (walk) since 21:00, 5 min left', True).assert_not_called()

    def test_nothing_running_nothing_stopped(self):
        self.release('held by ela (walk) since 21:00, 5 min left', False).assert_not_called()

    def test_no_name_stops_nothing(self):
        self.release('free', True, who='').assert_not_called()


class Reap(unittest.TestCase):
    """a full cap first stops launchers whose lease is gone; held ones and ones with no pid of ours stay"""

    def test_only_unleased_launchers_with_a_pid_are_stopped(self):
        alive = {'dead-holder': True, 'held': True}
        leases = {'sb-dead-holder': 'free', 'sb-held': 'held by tola (walk) since 21:00, 5 min left'}
        stopped = []

        def stop(n):
            stopped.append(n)
            alive[n] = False

        with mock.patch.object(abvm, 'sb_running', side_effect=lambda n: {'pid': 7} if alive.get(n) else None), \
                mock.patch.object(abvm, 'sb_stop', side_effect=stop), \
                mock.patch.object(abvm, 'lease_op', side_effect=lambda op, *a: leases[abvm.LEASE_KEY]):
            kept = abvm.sb_reap_unleased(['dead-holder', 'held', 'no-state'])
        self.assertEqual(stopped, ['dead-holder'])
        self.assertEqual(kept, ['held', 'no-state'])
        self.assertEqual(abvm.LEASE_KEY, 'vm')


if __name__ == '__main__':
    unittest.main()
