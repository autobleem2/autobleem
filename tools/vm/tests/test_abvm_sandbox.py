"""abvm's sandbox creation: runs on any host, touches no VM (python -m unittest discover tools/vm/tests)"""
import os
import shutil
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


def write(path, text=''):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()


def cp_r(cmd, **kw):
    """`cp -r <src> <dst dir>` the way the sandbox code calls it, without a cp on the host"""
    src, dst = cmd[-2], cmd[-1]
    shutil.copytree(src, os.path.join(dst, os.path.basename(src)), dirs_exist_ok=True)


class Lay(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.host = self.tmp.name
        self.sb = os.path.join(self.host, 'sb1')
        self.patches = [mock.patch.object(abvm, 'SB_HOST', self.host),
                        mock.patch.object(abvm, 'sb_open_modes', lambda path: None),
                        mock.patch.object(abvm.subprocess, 'run', side_effect=cp_r)]
        for p in self.patches:
            p.start()

    def tearDown(self):
        for p in self.patches:
            p.stop()
        self.tmp.cleanup()

    def config(self):
        return os.path.join(self.sb, 'Autobleem', 'bin', 'autobleem', 'config.ini')

    def test_set_replaces_a_key_whatever_its_case_and_adds_a_new_one(self):
        write(self.config(), 'theme=ab2\nShowingTimeout=2\n')
        abvm.sb_set_config('sb1', ['Theme=ab2.0.0', 'Language=Polski'])
        self.assertEqual(read(self.config()), 'Theme=ab2.0.0\nShowingTimeout=2\nLanguage=Polski\n')

    def test_set_without_a_key_is_refused(self):
        write(self.config(), 'Theme=ab2\n')
        with self.assertRaises(abvm.Fail):
            abvm.sb_set_config('sb1', ['Polski'])

    def test_package_lays_launcher_themes_extensions_and_version(self):
        pkg = os.path.join(self.host, 'pkg', 'autobleem-pcusb')
        write(os.path.join(pkg, 'Autobleem', 'bin', 'autobleem', 'config.ini'), 'Theme=ab2.0.0\n')
        write(os.path.join(pkg, 'Themes', 'ab2.0.0', 'theme.ini'), 'new')
        write(os.path.join(pkg, 'extensions', 'store', 'extension.ini'), 'Version=1.0.2\n')
        write(os.path.join(pkg, 'VERSION'), 'v2.0.0-alpha1\n')
        write(os.path.join(self.sb, 'Themes', 'ab2.0.0', 'stale.ini'), 'old')
        write(os.path.join(self.sb, 'Themes', 'ab2', 'theme.ini'), 'kept')
        write(os.path.join(self.sb, 'Extensions', 'store', 'extension.ini'), 'Version=1.0.1\n')
        write(os.path.join(self.sb, 'VERSION'), 'v2.0.0-alpha2-314\n')
        abvm.sb_copy_package('sb1', pkg)
        self.assertEqual(read(self.config()), 'Theme=ab2.0.0\n')
        self.assertTrue(os.path.isfile(os.path.join(self.sb, 'Themes', 'ab2.0.0', 'theme.ini')))
        self.assertFalse(os.path.exists(os.path.join(self.sb, 'Themes', 'ab2.0.0', 'stale.ini')))
        self.assertEqual(read(os.path.join(self.sb, 'Themes', 'ab2', 'theme.ini')), 'kept')
        self.assertEqual(read(os.path.join(self.sb, 'Extensions', 'store', 'extension.ini')), 'Version=1.0.2\n')
        self.assertEqual(read(os.path.join(self.sb, 'VERSION')), 'v2.0.0-alpha1\n')

    def test_package_without_a_launcher_is_refused(self):
        os.makedirs(os.path.join(self.host, 'empty'))
        with self.assertRaises(abvm.Fail):
            abvm.sb_copy_package('sb1', os.path.join(self.host, 'empty'))

    def test_games_bring_memory_cards_and_apps_for_the_pc_stick(self):
        for card in ('card1.mcd', 'card2.mcd'):
            write(os.path.join(self.sb, 'Autobleem', 'bin', 'autobleem', 'memcard', card), 'blank')
        with mock.patch('builtins.print'):
            abvm.sb_add_games('sb1', 3)
        games = os.path.join(self.sb, 'Games')
        cues = [f for _, _, files in os.walk(games) for f in files if f.endswith('.cue')]
        self.assertEqual(len(cues), 3)
        self.assertTrue(os.path.isfile(os.path.join(games, '!MemCards', 'Kids', 'card1.mcd')))
        self.assertTrue(os.path.isfile(os.path.join(self.sb, 'Apps', 'MoviePlayer', 'bin', 'pcusb', 'MoviePlayer')))


if __name__ == '__main__':
    unittest.main()
