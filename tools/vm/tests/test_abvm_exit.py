"""abvm's launcher exit record, exit-status and crash-report: runs on any host, touches no VM"""
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import abvm  # noqa: E402


class Parse(unittest.TestCase):
    def test_signal_status_is_split_into_exit_and_signal(self):
        self.assertEqual(abvm.parse_exit_record('134 1760000000\n'),
                         {'launcher_exit': 134, 'launcher_signal': 6, 'finished_at': 1760000000})

    def test_plain_exit_has_no_signal(self):
        self.assertEqual(abvm.parse_exit_record('0 5'), {'launcher_exit': 0, 'launcher_signal': None, 'finished_at': 5})
        self.assertIsNone(abvm.parse_exit_record('128 5')['launcher_signal'])

    def test_empty_or_garbage_is_all_null(self):
        for text in ('', None, 'x', '134'):
            self.assertEqual(abvm.parse_exit_record(text),
                             {'launcher_exit': None, 'launcher_signal': None, 'finished_at': None})

    def test_env_words(self):
        self.assertEqual(abvm.sb_check_env_words(['AB_FRAME_STATS=1', 'MALLOC_CHECK_=3', 'ASAN_OPTIONS=a=b']),
                         ['AB_FRAME_STATS=1', 'MALLOC_CHECK_=3', 'ASAN_OPTIONS=a=b'])
        for bad in ('LD_PRELOAD=x.so', 'LD_LIBRARY_PATH=/x', 'PATH=/x', 'AB_X'):
            with self.assertRaises(abvm.Fail):
                abvm.sb_check_env_words([bad])

    def test_flags(self):
        abvm.check_sandbox_flags(['--valgrind', '--size', '800x600'])
        with self.assertRaises(abvm.Fail):
            abvm.check_sandbox_flags(['--valgrnd'])

    def test_run_script_is_valid_shell(self):
        r = subprocess.run(['sh', '-n'], input=abvm.RUN_SCRIPT_TEXT, text=True, capture_output=True)
        self.assertEqual(r.returncode, 0, r.stderr)


class Commands(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.host = self.tmp.name
        self.ab = os.path.join(self.host, 'sb2', '.abvm')
        os.makedirs(self.ab)
        self.patches = [mock.patch.object(abvm, 'SB_HOST', self.host),
                        mock.patch.object(abvm, 'LOCAL', True),
                        mock.patch.object(abvm, 'sb_running', lambda n: None)]
        for p in self.patches:
            p.start()

    def tearDown(self):
        for p in self.patches:
            p.stop()
        self.tmp.cleanup()

    def say(self, *args):
        out = io.StringIO()
        with redirect_stdout(out):
            abvm.sandbox_command(list(args), '.')
        return out.getvalue()

    def record(self, text):
        with open(os.path.join(self.ab, abvm.EXIT_FILE), 'w') as f:
            f.write(text)

    def test_exit_status_json(self):
        self.record('134 1760000000\n')
        self.assertEqual(json.loads(self.say('exit-status', 'sb2')),
                         {'launcher_exit': 134, 'launcher_signal': 6, 'finished_at': 1760000000})

    def test_exit_status_before_any_run_is_null(self):
        self.assertEqual(json.loads(self.say('exit-status', 'sb2')),
                         {'launcher_exit': None, 'launcher_signal': None, 'finished_at': None})

    def test_clear_run_record_removes_status_and_core(self):
        self.record('0 1')
        os.makedirs(os.path.join(self.ab, 'core'))
        open(os.path.join(self.ab, 'core', 'core.1'), 'w').close()
        abvm.sb_clear_run_record('sb2')
        self.assertEqual(os.listdir(self.ab), [])

    def test_crash_report_clean_exit(self):
        self.record('0 1')
        self.assertIn('no crash', self.say('crash-report', 'sb2'))

    def test_crash_report_no_core_names_the_core_pattern(self):
        self.record('134 1')
        with mock.patch.object(abvm, 'guest_run', return_value='|/usr/lib/systemd/systemd-coredump %P\n'):
            text = self.say('crash-report', 'sb2')
        self.assertIn('signal 6', text)
        self.assertIn('no core file', text)
        self.assertIn('piped', text)

    def test_crash_report_no_gdb(self):
        self.record('134 1')
        os.makedirs(os.path.join(self.ab, 'core'))
        open(os.path.join(self.ab, 'core', 'core'), 'w').close()
        with mock.patch.object(abvm, 'guest_run', return_value=''):
            self.assertIn('gdb is not installed', self.say('crash-report', 'sb2'))

    def test_crash_report_backtrace(self):
        self.record('134 1')
        os.makedirs(os.path.join(self.ab, 'core'))
        open(os.path.join(self.ab, 'core', 'core.7'), 'w').close()
        calls = []

        def guest(cmd, check=True):
            calls.append(cmd)
            return '/usr/bin/gdb' if cmd.startswith('command -v') else '#0  raise () at x.c:1\n#1  main () at main.cpp:9\n'

        with mock.patch.object(abvm, 'guest_run', guest):
            text = self.say('crash-report', 'sb2')
        self.assertIn('#1  main () at main.cpp:9', text)
        self.assertIn('Autobleem/bin/autobleem/autobleem-gui', calls[-1])
        self.assertIn('/sb2/.abvm/core/core.7', calls[-1])

    def _core_and_gdb(self, reply):
        self.record('134 1')
        os.makedirs(os.path.join(self.ab, 'core'))
        open(os.path.join(self.ab, 'core', 'core'), 'w').close()
        calls = []

        def guest(cmd, check=True):
            calls.append(cmd)
            return '/usr/bin/gdb' if cmd.startswith('command -v') else reply

        with mock.patch.object(abvm, 'guest_run', guest):
            return self.say('crash-report', 'sb2'), calls

    def test_crash_report_opens_the_core_and_runs_gdb_as_root(self):
        text, calls = self._core_and_gdb('#0  raise ()\n')
        self.assertIn('sudo chmod -R a+rX', calls[-1])
        self.assertIn('sudo gdb', calls[-1])
        self.assertIn('#0  raise ()', text)

    def test_crash_report_unreadable_core_names_the_reason(self):
        text, _ = self._core_and_gdb('/x/core: Permission denied.\n')
        self.assertIn('could not read the core', text)
        self.assertIn('Permission denied', text)
        self.assertNotIn('#0', text)

    def test_crash_report_no_frames_says_what_gdb_said(self):
        text, _ = self._core_and_gdb('warning: core file may not match\n')
        self.assertIn('no backtrace: gdb gave no frames', text)
        self.assertIn('may not match', text)

    def test_run_script_writes_the_core_on_the_guest_disk_then_copies_it(self):
        t = abvm.RUN_SCRIPT_TEXT
        self.assertIn('kernel.core_pattern="$local/core.%e.%p"', t)
        self.assertIn('local=/var/tmp/abvm-core', t)
        self.assertIn('cp -f "$c" "$ab/core/"', t)
        self.assertIn('wait "$pid"', t)

    def test_run_script_opens_the_kept_core(self):
        self.assertIn('chmod a+rw "$ab/core"/*', abvm.RUN_SCRIPT_TEXT)

    def test_crash_report_while_running(self):
        with mock.patch.object(abvm, 'sb_running', lambda n: {'pid': 1}):
            self.assertIn('runs', self.say('crash-report', 'sb2'))


class Signal(unittest.TestCase):
    def sent(self, name, sig, running=True, reply='sent'):
        calls = []
        out = io.StringIO()
        with mock.patch.object(abvm, 'LOCAL', True), mock.patch.object(abvm, 'WHO', 'alice'), \
                mock.patch.object(abvm, 'lease_op', lambda *a, **k: 'ok'), \
                mock.patch.object(abvm, 'sb_running', lambda n: {'pid': 4242} if running else None), \
                mock.patch.object(abvm, 'guest_run', lambda cmd, check=True: calls.append(cmd) or reply), \
                redirect_stdout(out):
            abvm.sandbox_command(['signal', name, sig], '.')
        return calls, out.getvalue()

    def test_each_allowed_signal_is_sent_to_the_launcher_pid(self):
        for sig in ('ABRT', 'SEGV', 'TERM', 'KILL', 'INT'):
            calls, out = self.sent('sb2', sig)
            self.assertEqual(len(calls), 1)
            self.assertIn(f'kill -0 4242 2>/dev/null && kill -{sig} 4242', calls[0])
            self.assertIn(f'SIG{sig} sent', out)

    def test_a_launcher_that_died_or_refuses_the_kill_is_reported_gone(self):
        calls, out = self.sent('sb2', 'ABRT', reply='gone\n')
        self.assertIn('launcher already gone', out)
        self.assertNotIn('sent to the launcher', out)

    def test_name_forms_are_normalised(self):
        self.assertIn('kill -ABRT 4242', self.sent('sb2', 'sigabrt')[0][0])
        self.assertIn('kill -TERM 4242', self.sent('sb2', 'term')[0][0])

    def test_other_signals_are_refused_and_nothing_is_sent(self):
        for sig in ('HUP', 'STOP', '9', 'USR1', '', 'ABRT; reboot', '-KILL'):
            calls = []
            with self.assertRaises(abvm.Fail) as cm:
                calls = self.sent('sb2', sig)[0]
            self.assertIn('not allowed', str(cm.exception))
            self.assertEqual(calls, [])

    def test_no_running_launcher_is_an_error(self):
        with self.assertRaises(abvm.Fail) as cm:
            self.sent('sb2', 'TERM', running=False)
        self.assertIn('no launcher runs', str(cm.exception))

    def test_a_name_outside_the_pool_is_refused(self):
        with self.assertRaises(abvm.Fail):
            self.sent('sb1', 'TERM')

    def test_usage_without_a_signal(self):
        with mock.patch.object(abvm, 'LOCAL', True), self.assertRaises(abvm.Fail):
            abvm.sandbox_command(['signal', 'sb2'], '.')


class DebugTools(unittest.TestCase):
    def test_setup_installs_valgrind_and_gdb_through_sudo_apt(self):
        calls = []
        with mock.patch.object(abvm, 'guest_run', lambda cmd, check=True: calls.append(cmd) or ''):
            abvm.sb_install_debug_tools()
        self.assertEqual(len(calls), 1)
        for word in ('valgrind', 'gdb', 'sudo apt-get', 'install -y $missing'):
            self.assertIn(word, calls[0])
        self.assertIn('command -v valgrind >/dev/null ||', calls[0])
        r = subprocess.run(['sh', '-n'], input=calls[0], text=True, capture_output=True)
        self.assertEqual(r.returncode, 0, r.stderr)

    def setup(self, tools):
        out = io.StringIO()
        with mock.patch.object(abvm, 'guest_run', return_value=''), \
                mock.patch.object(abvm, 'sb_host', lambda n='': '/nonexistent-abvm-test'), \
                mock.patch.object(abvm.os, 'makedirs'), mock.patch.object(abvm, 'sb_share_group'), \
                mock.patch.object(abvm, 'sb_install_debug_tools', tools), redirect_stdout(out):
            abvm.sb_setup()
        return out.getvalue()

    def test_sb_setup_runs_it(self):
        tools = mock.Mock()
        self.setup(tools)
        tools.assert_called_once_with()

    def test_a_failed_install_is_a_warning_after_the_summary(self):
        out = self.setup(mock.Mock(side_effect=abvm.Fail('apt: guest offline')))
        lines = out.splitlines()
        self.assertTrue(lines[0].startswith('sandboxes: '))
        self.assertEqual(lines[1], 'WARNING: valgrind/gdb not installed: apt: guest offline; '
                                   '--valgrind and crash-report will say so')


if __name__ == '__main__':
    unittest.main()
