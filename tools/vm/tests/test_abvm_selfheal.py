"""abvm keeps the test machine's copy current: no VM, no ssh (python -m unittest discover tools/vm/tests)"""
import contextlib
import io
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import abvm  # noqa: E402


class SelfHeal(unittest.TestCase):
    def setUp(self):
        self.calls = []
        self.copied = []
        self.patches = [
            mock.patch.object(abvm, '_remote_checked', False),
            mock.patch.object(abvm, 'LOCAL', False),
            mock.patch.object(abvm, 'host_run', side_effect=self.host_run),
            mock.patch.object(abvm, 'to_host', side_effect=lambda local, remote: self.copied.append(remote)),
        ]
        for p in self.patches:
            p.start()
        self.remote = abvm.tool_version()

    def tearDown(self):
        for p in self.patches:
            p.stop()

    def host_run(self, command, check=True, full=False):
        self.calls.append(command)
        if 'sha256sum' in command:
            return ''.join(f'{h}  {n}\n' for n, h in self.remote.items())
        if full:
            return mock.Mock(stdout='', stderr='', returncode=0)
        return ''

    def refreshes(self):
        return [c for c in self.calls if c.startswith('mkdir -p')]

    def test_matching_versions_skip_the_refresh(self):
        abvm.remote_tool(['lock', 'show'])
        self.assertEqual(self.refreshes(), [])
        self.assertEqual(self.copied, [])
        self.assertTrue(any('lock show' in c for c in self.calls))

    def test_a_stale_file_triggers_one_refresh_before_the_call(self):
        self.remote['ab_drive.py'] = '0' * 64
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            abvm.remote_tool(['lock', 'show'])
            abvm.remote_tool(['lock', 'show'])
        self.assertEqual(len(self.refreshes()), 1)           # once per invocation
        self.assertIn('ab_drive.py', err.getvalue())
        files = len(abvm.local_sources())
        self.assertEqual(len(self.copied), files)            # every file is sent...
        self.assertTrue(all('.new' in c for c in self.copied))  # ...under temp names
        mv = [c for c in self.calls if c.startswith('mv -f')][0]
        self.assertEqual(mv.count('mv -f'), files)           # ...and renamed into place
        self.assertTrue(mv.rstrip().split('&&')[-1].strip().endswith('abvm.py'))  # the tool last
        order = [i for i, c in enumerate(self.calls) if c.startswith('mkdir -p') or 'lock show' in c]
        self.assertLess(order[0], order[1])                  # refreshed before the real call

    def test_a_missing_remote_copy_is_refreshed(self):
        self.remote = {}
        with contextlib.redirect_stderr(io.StringIO()):
            abvm.remote_tool(['lock', 'show'])
        self.assertEqual(len(self.refreshes()), 1)

    def tool_command(self):
        abvm.remote_tool(['lock', 'show'])
        return [c for c in self.calls if 'lock show' in c][0]

    def test_pythonpath_is_added_only_when_set(self):
        with mock.patch.object(abvm, 'PYTHONPATH', ''):
            self.assertNotIn('PYTHONPATH', self.tool_command())

    def test_pythonpath_is_quoted_into_the_remote_command(self):
        with mock.patch.object(abvm, 'PYTHONPATH', '/opt/py libs'):
            command = self.tool_command()
        self.assertTrue(command.startswith("env 'PYTHONPATH=/opt/py libs' python3 "))

    def test_chmod_noise_is_one_summary_line(self):
        noise = ''.join(f"chmod: changing permissions of '/x/{i}': Operation not permitted\n" for i in range(50))
        done = mock.Mock(stderr=noise + "chmod: cannot access '/x/gone': No such file or directory\n")
        err = io.StringIO()
        with mock.patch.object(abvm.subprocess, 'run', return_value=done), contextlib.redirect_stderr(err):
            abvm.sb_open_modes('/x')
        lines = err.getvalue().splitlines()
        self.assertEqual(len(lines), 2)
        self.assertIn('50 files could not be opened up', lines[-1])
        self.assertIn('No such file', lines[0])              # a different error stays visible

    def test_on_the_test_machine_itself_nothing_is_checked(self):
        with mock.patch.object(abvm, 'LOCAL', True):
            abvm.ensure_remote_current()
        self.assertEqual(self.calls, [])

    def test_the_version_ignores_crlf(self):
        with tempfile.TemporaryDirectory() as d:
            a, b = os.path.join(d, 'a'), os.path.join(d, 'b')
            with open(a, 'wb') as f:
                f.write(b'x = 1\r\ny = 2\r\n')
            with open(b, 'wb') as f:
                f.write(b'x = 1\ny = 2\n')
            self.assertEqual(abvm._normalised(a), abvm._normalised(b))


class Strict(unittest.TestCase):
    def run_main(self, argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err), \
                mock.patch.object(abvm, 'vm_prepare', lambda cmd, args: None):  # no VM to wake here
            rc = abvm.main(argv)
        return rc, out.getvalue(), err.getvalue()

    def test_an_unknown_command_fails_loudly(self):
        rc, out, err = self.run_main(['--local', 'frobnicate'])
        self.assertEqual(rc, 2)
        self.assertEqual(out, '')
        self.assertIn("unknown command 'frobnicate'", err)

    def test_an_unknown_sandbox_flag_is_an_error(self):
        rc, _, err = self.run_main(['--local', 'sandbox', 'start', 'sb2', '--gamez', '12'])
        self.assertEqual(rc, 1)
        self.assertIn('--gamez', err)

    def test_a_flag_without_its_value_is_an_error(self):
        rc, _, err = self.run_main(['--local', 'sandbox', 'new', 'sb2', '--games'])
        self.assertEqual(rc, 1)
        self.assertIn('needs a value', err)

    def test_known_flags_pass_the_check(self):
        abvm.check_sandbox_flags(['--games', '12', '--ext', 'a', '--ext', 'b', '--set', 'Language=Polski'])


if __name__ == '__main__':
    unittest.main()
