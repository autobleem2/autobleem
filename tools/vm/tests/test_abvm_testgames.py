"""abvm sandbox --games test: the test machine's real game set laid into a sandbox (discs hard-linked)"""
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import abvm  # noqa: E402


class TestGames(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.set = os.path.join(self.tmp.name, 'testgames')
        self.host = os.path.join(self.tmp.name, 'sandboxes')
        game = os.path.join(self.set, 'Game One')
        os.makedirs(game)
        with open(os.path.join(game, 'disc.bin'), 'wb') as f:
            f.write(b'\0' * (abvm.LINK_MIN_BYTES + 1))
        with open(os.path.join(game, 'Game.ini'), 'w') as f:
            f.write('[Game]\nTitle=One\n')
        os.makedirs(os.path.join(self.host, 'sb2', 'Games'))
        self.patches = [mock.patch.object(abvm, 'SB_HOST', self.host), mock.patch.object(abvm, 'LOCAL', True),
                        mock.patch.dict(os.environ, {'ABVM_TESTGAMES': self.set}),
                        mock.patch.object(abvm, 'sb_open_modes', lambda p: None)]
        for p in self.patches:
            p.start()

    def tearDown(self):
        for p in self.patches:
            p.stop()
        self.tmp.cleanup()

    def test_the_disc_is_linked_and_the_small_files_copied(self):
        abvm.sb_lay('sb2', games=abvm.parse_games('test'))
        dest = os.path.join(self.host, 'sb2', 'Games', 'Game One')
        disc, ini = os.path.join(dest, 'disc.bin'), os.path.join(dest, 'Game.ini')
        self.assertTrue(os.path.samefile(disc, os.path.join(self.set, 'Game One', 'disc.bin')))
        self.assertFalse(os.path.samefile(ini, os.path.join(self.set, 'Game One', 'Game.ini')))
        with open(ini, 'a') as f:   # the launcher's write stays in this sandbox
            f.write('Changed=1\n')
        with open(os.path.join(self.set, 'Game One', 'Game.ini')) as f:
            self.assertNotIn('Changed', f.read())

    def test_a_folder_already_there_is_kept(self):
        dest = os.path.join(self.host, 'sb2', 'Games', 'Game One')
        os.makedirs(dest)
        abvm.sb_add_test_games('sb2')
        self.assertEqual(os.listdir(dest), [])

    def test_no_set_is_an_error(self):
        with mock.patch.dict(os.environ, {'ABVM_TESTGAMES': os.path.join(self.tmp.name, 'none')}):
            with self.assertRaises(abvm.Fail):
                abvm.sb_add_test_games('sb2')

    def test_games_values(self):
        self.assertEqual(abvm.parse_games(None), 0)
        self.assertEqual(abvm.parse_games('12'), 12)
        self.assertEqual(abvm.parse_games('test'), 'test')
        for bad in ('-1', 'all', '../x', '1000'):
            with self.assertRaises(abvm.Fail):
                abvm.parse_games(bad)


if __name__ == '__main__':
    unittest.main()
