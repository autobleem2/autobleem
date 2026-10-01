"""abvm's sandbox creation: runs on any host, touches no VM (python -m unittest discover tools/vm/tests)"""
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import abvm  # noqa: E402


class StoreDirs(unittest.TestCase):
    def test_new_sandbox_has_the_store_state_dirs_opened(self):
        with tempfile.TemporaryDirectory() as host:
            opened = []
            with mock.patch.object(abvm, 'SB_HOST', host), \
                    mock.patch.object(abvm, 'sb_open_modes', opened.append), \
                    mock.patch.object(abvm.subprocess, 'run', side_effect=lambda cmd, **kw: os.makedirs(cmd[-1])):
                os.makedirs(os.path.join(host, '_template'))
                abvm.sb_new('sb1')
            for d in ('cache', 'downloads', 'staging', 'sources'):
                self.assertTrue(os.path.isdir(os.path.join(host, 'sb1', 'System', 'Extensions', 'store', d)), d)
            self.assertIn(os.path.join(host, 'sb1', 'System', 'Extensions'), opened)


if __name__ == '__main__':
    unittest.main()
