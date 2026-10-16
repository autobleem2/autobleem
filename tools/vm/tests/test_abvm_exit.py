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
        self.ab = os.path.join(self.host, 'sb1', '.abvm')
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
        self.assertEqual(json.loads(self.say('exit-status', 'sb1')),
                         {'launcher_exit': 134, 'launcher_signal': 6, 'finished_at': 1760000000})

    def test_exit_status_before_any_run_is_null(self):
        self.assertEqual(json.loads(self.say('exit-status', 'sb1')),
                         {'launcher_exit': None, 'launcher_signal': None, 'finished_at': None})

    def test_clear_run_record_removes_status_and_core(self):
        self.record('0 1')
        os.makedirs(os.path.join(self.ab, 'core'))
        open(os.path.join(self.ab, 'core', 'core.1'), 'w').close()
        abvm.sb_clear_run_record('sb1')
        self.assertEqual(os.listdir(self.ab), [])

    def test_crash_report_clean_exit(self):
        self.record('0 1')
        self.assertIn('no crash', self.say('crash-report', 'sb1'))

    def test_crash_report_no_core_names_the_core_pattern(self):
        self.record('134 1')
        with mock.patch.object(abvm, 'guest_run', return_value='|/usr/lib/systemd/systemd-coredump %P\n'):
            text = self.say('crash-report', 'sb1')
        self.assertIn('signal 6', text)
        self.assertIn('no core file', text)
        self.assertIn('piped', text)

    def test_crash_report_no_gdb(self):
        self.record('134 1')
        os.makedirs(os.path.join(self.ab, 'core'))
        open(os.path.join(self.ab, 'core', 'core'), 'w').close()
        with mock.patch.object(abvm, 'guest_run', return_value=''):
            self.assertIn('gdb is not installed', self.say('crash-report', 'sb1'))

    def test_crash_report_backtrace(self):
        self.record('134 1')
        os.makedirs(os.path.join(self.ab, 'core'))
        open(os.path.join(self.ab, 'core', 'core.7'), 'w').close()
        calls = []

        def guest(cmd, check=True):
            calls.append(cmd)
            return '/usr/bin/gdb' if cmd.startswith('command -v') else '#0  raise () at x.c:1\n#1  main () at main.cpp:9\n'

        with mock.patch.object(abvm, 'guest_run', guest):
            text = self.say('crash-report', 'sb1')
        self.assertIn('#1  main () at main.cpp:9', text)
        self.assertIn('Autobleem/bin/autobleem/autobleem-gui', calls[-1])
        self.assertIn('/sb1/.abvm/core/core.7', calls[-1])

    def test_crash_report_while_running(self):
        with mock.patch.object(abvm, 'sb_running', lambda n: {'pid': 1}):
            self.assertIn('runs', self.say('crash-report', 'sb1'))


if __name__ == '__main__':
    unittest.main()
