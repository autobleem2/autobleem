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
            store = os.path.join(host, 'sb1', 'System', 'Extensions', 'store')
            for d in ('cache', os.path.join('cache', 'pictures'), 'downloads', 'staging', 'sources'):
                self.assertTrue(os.path.isdir(os.path.join(store, d)), d)
            self.assertIn(os.path.join(host, 'sb1', 'System', 'Extensions'), opened)

    def test_new_sandbox_has_mods_and_apps_made_and_opened(self):
        with tempfile.TemporaryDirectory() as host:
            opened = []
            with mock.patch.object(abvm, 'SB_HOST', host),                     mock.patch.object(abvm, 'sb_open_modes', opened.append),                     mock.patch.object(abvm.subprocess, 'run', side_effect=lambda cmd, **kw: os.makedirs(cmd[-1])):
                os.makedirs(os.path.join(host, '_template'))
                abvm.sb_new('sb1')
            for d in ('Mods', 'Apps'):
                self.assertTrue(os.path.isdir(os.path.join(host, 'sb1', d)), d)
                self.assertIn(os.path.join(host, 'sb1', d), opened)

    def test_existing_dirs_are_kept_not_failed_on(self):
        with tempfile.TemporaryDirectory() as host:
            os.makedirs(os.path.join(host, 'sb1', 'Mods'))
            with mock.patch.object(abvm, 'SB_HOST', host), mock.patch.object(abvm, 'sb_open_modes', lambda p: None):
                abvm.sb_make_store_dirs('sb1')
                abvm.sb_make_store_dirs('sb1')
            self.assertTrue(os.path.isdir(os.path.join(host, 'sb1', 'Mods')))

    def test_reset_recreates_the_dirs(self):
        with tempfile.TemporaryDirectory() as host:
            calls = []
            with mock.patch.object(abvm, 'SB_HOST', host),                     mock.patch.object(abvm, 'sb_open_modes', lambda p: None),                     mock.patch.object(abvm.subprocess, 'run', side_effect=lambda cmd, **kw: os.makedirs(cmd[-1])),                     mock.patch.object(abvm, 'sb_stop', lambda n: calls.append('stop')),                     mock.patch.object(abvm, 'sb_remove', lambda n: shutil.rmtree(abvm.sb_host(n))):
                os.makedirs(os.path.join(host, '_template'))
                abvm.sb_new('sb1')
                abvm.sb_stop('sb1')
                abvm.sb_remove('sb1')
                self.assertFalse(os.path.exists(os.path.join(host, 'sb1')))
                abvm.sb_new('sb1')
            self.assertTrue(os.path.isdir(os.path.join(host, 'sb1', 'System', 'Extensions', 'store', 'cache', 'pictures')))
            self.assertTrue(os.path.isdir(os.path.join(host, 'sb1', 'Mods')))


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


class OpenTree(unittest.TestCase):
    def test_pass_runs_in_the_guest_as_root_over_the_whole_sandbox(self):
        guest = []
        with mock.patch.object(abvm, 'SB_HOST', '/nonexistent-host'), \
                mock.patch.object(abvm, 'sb_open_modes', lambda p: None), \
                mock.patch.object(abvm, 'guest_run', lambda cmd, check=True: guest.append((cmd, check))):
            abvm.sb_open_tree('sb1')
        self.assertEqual(len(guest), 1)
        cmd, check = guest[0]
        self.assertFalse(check)  # a failed pass must not fail the run
        self.assertIn('sudo find ' + abvm.SB_GUEST + '/sb1 ', cmd)
        self.assertIn('chmod a+rwX', cmd)
        self.assertIn('-type d', cmd)
        self.assertIn('-type f', cmd)

    def test_default_acl_only_when_asked_and_a_missing_setfacl_is_no_error(self):
        runs = []
        with mock.patch.object(abvm, 'SB_HOST', '/nonexistent-host'), \
                mock.patch.object(abvm, 'sb_open_modes', lambda p: None), \
                mock.patch.object(abvm, 'guest_run', lambda cmd, check=True: ''), \
                mock.patch.object(abvm.subprocess, 'run', side_effect=lambda cmd, **kw: runs.append(cmd)):
            abvm.sb_open_tree('sb1')
            self.assertEqual(runs, [])
            abvm.sb_open_tree('sb1', acl=True)
        self.assertEqual(runs[0][:4], ['setfacl', '-R', '-d', '-m'])
        self.assertEqual(runs[0][-1], os.path.join('/nonexistent-host', 'sb1'))
        with mock.patch.object(abvm, 'SB_HOST', '/nonexistent-host'), \
                mock.patch.object(abvm, 'sb_open_modes', lambda p: None), \
                mock.patch.object(abvm, 'guest_run', lambda cmd, check=True: ''), \
                mock.patch.object(abvm.subprocess, 'run', side_effect=FileNotFoundError):
            abvm.sb_open_tree('sb1', acl=True)

    def test_sandbox_drive_opens_the_tree_before_every_step(self):
        with tempfile.TemporaryDirectory() as host:
            os.makedirs(os.path.join(host, 'sb1'))
            opened = []

            class FakeDriver:
                def __init__(self, before_step):
                    self.before_step = before_step

                def run(self, script):
                    for _ in script.split(';'):
                        self.before_step()
                    return []

                def close(self):
                    pass

            def fake_driver(name, **kw):
                return FakeDriver(kw['before_step'])

            with mock.patch.object(abvm, 'SB_HOST', host), mock.patch.object(abvm, 'sb_driver', fake_driver), \
                    mock.patch.object(abvm, 'sb_open_tree', lambda n, acl=False: opened.append(n)), \
                    mock.patch.object(abvm, 'sb_open_modes', lambda p: None):
                abvm.sb_drive('sb1', '@1 tap a; wait_idle 5; shot a.png', host)
            self.assertEqual(opened, ['sb1'] * 3)


class Tour(unittest.TestCase):
    def run_tour(self, host, outdir, prev=None, rc=0):
        calls = []

        class FakeTour:
            @staticmethod
            def main(argv, shot_path=None, ra_cores=True):
                calls.append((argv, shot_path, ra_cores))
                return rc

        with mock.patch.object(abvm, 'SB_HOST', host), mock.patch.object(abvm, 'import_ab_drive', lambda: None), \
                mock.patch.dict(sys.modules, {'ui_tour': FakeTour}), \
                mock.patch.object(abvm, 'sb_running', lambda n: {'port': 7801}), \
                mock.patch.object(abvm, 'sb_open_tree', lambda n, acl=False: None), \
                mock.patch.object(abvm, 'sb_open_modes', lambda p: None):
            abvm.sb_tour('sb1', outdir, prev)
        return calls

    def test_command_line_attach_port_out_folder_and_guest_path(self):
        with tempfile.TemporaryDirectory() as host:
            calls = self.run_tour(host, 'tour1')
            argv, shot_path, _ = calls[0]
            out = os.path.join(host, 'sb1', '.abvm', 'out', 'tour1')
            self.assertEqual(argv, ['ui_tour.py', out, '--attach', '--port', '7801'])
            self.assertTrue(os.path.isdir(out))
            self.assertEqual(shot_path(os.path.join(out, '01-a.png')),
                             abvm.SB_GUEST + '/sb1/.abvm/out/tour1/01-a.png')

    def test_diff_option_is_passed_as_a_folder_of_the_out_folder(self):
        with tempfile.TemporaryDirectory() as host:
            os.makedirs(os.path.join(host, 'sb1', '.abvm', 'out', 'tour1'))
            argv = self.run_tour(host, 'tour2', 'tour1')[0][0]
            self.assertEqual(argv[-2:], ['--diff', os.path.join(host, 'sb1', '.abvm', 'out', 'tour1')])

    def test_refusals(self):
        with tempfile.TemporaryDirectory() as host:
            for bad in ('', '/etc', '../x', 'a/../../b', 'C:\\x'):
                with self.assertRaises(abvm.Fail):
                    self.run_tour(host, bad)
            with self.assertRaises(abvm.Fail):  # a previous tour that does not exist
                self.run_tour(host, 'tour2', 'nothere')
            with self.assertRaises(abvm.Fail):
                self.run_tour(host, 'tour2', '../tour1')
            with self.assertRaises(abvm.Fail):  # the tour's own failure
                self.run_tour(host, 'tour3', rc=1)

    def test_args(self):
        self.assertEqual(abvm.sb_tour_args(['o']), ('o', None))
        self.assertEqual(abvm.sb_tour_args(['o', '--diff', 'p']), ('o', 'p'))
        for bad in ([], ['o', '--diff'], ['o', 'p'], ['--diff', 'p'], ['o', '--diff', 'p', 'q'], ['o', '--bogus', 'p']):
            with self.assertRaises(abvm.Fail):
                abvm.sb_tour_args(bad)

    def test_the_tour_is_told_whether_the_sandbox_has_a_core(self):
        for cores in (False, True):
            with tempfile.TemporaryDirectory() as host:
                if cores:
                    d = os.path.join(host, 'sb1', 'RetroArch', 'cores')
                    os.makedirs(d)
                    open(os.path.join(d, 'fceumm_libretro.so'), 'w').close()
                self.assertEqual(self.run_tour(host, 'tour1')[0][2], cores)

    def test_core_detection(self):
        with tempfile.TemporaryDirectory() as host, mock.patch.object(abvm, 'SB_HOST', host):
            self.assertFalse(abvm.sb_has_ra_core('sb1'))
            cores = os.path.join(host, 'sb1', 'RetroArch', 'cores')
            os.makedirs(cores)
            self.assertFalse(abvm.sb_has_ra_core('sb1'))
            open(os.path.join(cores, 'fceumm_libretro.so'), 'w').close()
            self.assertTrue(abvm.sb_has_ra_core('sb1'))

    def test_not_running(self):
        with tempfile.TemporaryDirectory() as host, mock.patch.object(abvm, 'SB_HOST', host), \
                mock.patch.object(abvm, 'sb_running', lambda n: None):
            with self.assertRaises(abvm.Fail):
                abvm.sb_tour('sb1', 'o')


class TestMachineCopy(unittest.TestCase):
    def test_copy_list_has_the_unshipped_themes_files(self):
        names = [n for n, _ in abvm.local_sources()]
        self.assertIn('unshipped_themes.py', names)
        self.assertIn('unshipped_themes.txt', names)
        self.assertEqual(names[-1], os.path.basename(abvm.REMOTE_TOOL))
        self.assertTrue(all(os.path.exists(p) for _, p in abvm.local_sources()))
        self.assertEqual(sorted(abvm.tool_version()), sorted(names))

    def test_make_usb_imports_from_the_copy_alone(self):
        import subprocess
        with tempfile.TemporaryDirectory() as stage:
            for name, path in abvm.local_sources():
                shutil.copy(path, os.path.join(stage, name))
            r = subprocess.run([sys.executable, '-I', '-c', 'import sys; sys.path.insert(0, sys.argv[1]); '
                                'import make_usb', stage], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)


class StartSets(unittest.TestCase):
    def test_updates_off_by_default(self):
        self.assertEqual(abvm.start_sets([]), ['updates=off'])
        self.assertEqual(abvm.start_sets(['Language=Polski']), ['updates=off', 'Language=Polski'])

    def test_own_updates_line_wins(self):
        self.assertEqual(abvm.start_sets(['updates=release']), ['updates=release'])
        self.assertEqual(abvm.start_sets(['Updates = beta']), ['Updates = beta'])


if __name__ == '__main__':
    unittest.main()
