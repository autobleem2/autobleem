#!/usr/bin/env python3
"""abvm's bookkeeping write (the last-use record) survives parallel callers and never fails `lock take`
(run by ctest, or: python3 this_file.py)"""

import contextlib
import importlib.util
import io
import os
import tempfile
import threading
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
ABVM = os.path.abspath(os.path.join(HERE, "..", "..", "tools", "vm", "abvm.py"))


def load_abvm():
    spec = importlib.util.spec_from_file_location("abvm_under_test", ABVM)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class LastUseTest(unittest.TestCase):
    def setUp(self):
        self.abvm = load_abvm()
        self.home = tempfile.mkdtemp()
        self.addCleanup(self.cleanup)
        patcher = mock.patch.dict(os.environ, {"HOME": self.home, "USERPROFILE": self.home})
        patcher.start()
        self.addCleanup(patcher.stop)
        self.state = os.path.join(self.home, ".local", "state", "abvm")

    def cleanup(self):
        for root, _dirs, files in os.walk(self.home, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
            os.rmdir(root)

    def test_parallel_updates(self):
        errors = []

        def worker():
            try:
                for _ in range(200):
                    self.abvm.atomic_write(os.path.join(self.state, "last-use"), "1")
                    self.abvm.touch_last_use()
            except Exception as e:  # noqa: BLE001 - the test reports any failure
                errors.append(e)

        os.makedirs(self.state, exist_ok=True)
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            threads = [threading.Thread(target=worker) for _ in range(2)]
            for t in threads:
                t.start()
            for t in threads:
                t.join()
        self.assertEqual(errors, [])
        self.assertNotIn("could not record", stderr.getvalue())
        with open(os.path.join(self.state, "last-use")) as f:
            self.assertGreater(float(f.read().strip()), 0)
        self.assertEqual([n for n in os.listdir(self.state) if n.endswith(".tmp")], [])

    def test_stale_temp_of_dead_pid_is_removed(self):
        os.makedirs(self.state)
        stale = os.path.join(self.state, "last-use.2147483646.abc.tmp")
        with open(stale, "w") as f:
            f.write("x")
        self.abvm.touch_last_use()
        self.assertFalse(os.path.exists(stale))

    def test_pid_alive_never_signals_on_windows(self):
        with mock.patch.object(os, "name", "nt"), mock.patch.object(os, "kill") as kill:
            self.assertTrue(self.abvm.pid_alive(2147483646))
            os.makedirs(self.state)
            stale = os.path.join(self.state, "last-use.2147483646.abc.tmp")
            with open(stale, "w") as f:
                f.write("x")
            self.abvm.clean_stale_temps(self.state)
            self.assertTrue(os.path.exists(stale))
        kill.assert_not_called()

    def test_lock_take_survives_failed_temp_write(self):
        real_open = open

        def failing_open(path, *args, **kwargs):
            if str(path).endswith(".tmp") and "last-use" in str(path):
                raise PermissionError("forced")
            return real_open(path, *args, **kwargs)

        self.abvm.LOCAL = True
        self.abvm.WHO = "tester"
        out, err = io.StringIO(), io.StringIO()
        with mock.patch.object(self.abvm, "vm_prepare", lambda cmd, args: self.abvm.touch_last_use()), \
                mock.patch("builtins.open", failing_open), \
                contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = self.abvm.main(["lock", "take", "a task", "5"])
        self.assertEqual(code, 0, err.getvalue())
        self.assertIn("taken by tester", out.getvalue())
        self.assertIn("could not record the last use", err.getvalue())
        self.assertFalse(os.path.exists(os.path.join(self.state, "last-use")))


if __name__ == "__main__":
    unittest.main()
