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
        self.assertEqual(len(self.copied), 3)                # all three files are sent...
        self.assertTrue(all('.new' in c for c in self.copied))  # ...under temp names
        mv = [c for c in self.calls if c.startswith('mv -f')][0]
        self.assertEqual(mv.count('mv -f'), 3)               # ...and renamed into place
        self.assertTrue(mv.rstrip().split('&&')[-1].strip().endswith('abvm.py'))  # the tool last
        order = [i for i, c in enumerate(self.calls) if c.startswith('mkdir -p') or 'lock show' in c]
        self.assertLess(order[0], order[1])                  # refreshed before the real call

    def test_a_missing_remote_copy_is_refreshed(self):
        self.remote = {}
        with contextlib.redirect_stderr(io.StringIO()):
            abvm.remote_tool(['lock', 'show'])
        self.assertEqual(len(self.refreshes()), 1)

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
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            rc = abvm.main(argv)
        return rc, out.getvalue(), err.getvalue()

    def test_an_unknown_command_fails_loudly(self):
        rc, out, err = self.run_main(['--local', 'frobnicate'])
        self.assertEqual(rc, 2)
        self.assertEqual(out, '')
        self.assertIn("unknown command 'frobnicate'", err)

    def test_an_unknown_sandbox_flag_is_an_error(self):
        rc, _, err = self.run_main(['--local', 'sandbox', 'start', 'sb1', '--gamez', '12'])
        self.assertEqual(rc, 1)
        self.assertIn('--gamez', err)

    def test_a_flag_without_its_value_is_an_error(self):
        rc, _, err = self.run_main(['--local', 'sandbox', 'new', 'sb1', '--games'])
        self.assertEqual(rc, 1)
        self.assertIn('needs a value', err)

    def test_known_flags_pass_the_check(self):
        abvm.check_sandbox_flags(['--games', '12', '--ext', 'a', '--ext', 'b', '--set', 'Language=Polski'])


if __name__ == '__main__':
    unittest.main()
