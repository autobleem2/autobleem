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
            def main(argv, shot_path=None):
                calls.append((argv, shot_path))
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
            argv, shot_path = calls[0]
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

    def test_not_running(self):
        with tempfile.TemporaryDirectory() as host, mock.patch.object(abvm, 'SB_HOST', host), \
                mock.patch.object(abvm, 'sb_running', lambda n: None):
            with self.assertRaises(abvm.Fail):
                abvm.sb_tour('sb1', 'o')


class StopFast(unittest.TestCase):
    """sb_stop on a fake guest: the launcher dies when the fake `kill` / `quit` says so"""

    def stop(self, state, dies_on, quit_raises=None):
        """dies_on: 'quit', 'TERM', 'KILL' or None (never); returns the log of what was done"""
        log, alive = [], [True]

        class Sock:
            def settimeout(self, t):
                log.append(f'timeout {t}')

        class Driver:
            sock = Sock()

            def cmd(self, line):
                log.append(line)
                if quit_raises:
                    raise quit_raises
                if dies_on == 'quit':
                    alive[0] = False

            def close(self):
                log.append('close')

        def guest_run(cmd, check=True):
            log.append(cmd)
            if dies_on and cmd.startswith(f'kill -{dies_on} '):
                alive[0] = False
            return ''

        def sleep(s):
            log.append('sleep')

        with mock.patch.object(abvm, 'sb_state', lambda n: dict(state)), \
                mock.patch.object(abvm, 'sb_guest_pid_alive', lambda n, p: alive[0]), \
                mock.patch.object(abvm, 'sb_driver', lambda n, **kw: log.append('driver') or Driver()), \
                mock.patch.object(abvm, 'guest_run', guest_run), mock.patch.object(abvm.time, 'sleep', sleep), \
                mock.patch.object(abvm, 'sb_stop_forward', lambda st: None), \
                mock.patch.object(abvm, 'sb_save_state', lambda n, st: None), \
                mock.patch.object(abvm, 'sb_orphans', lambda: {}), \
                mock.patch.object(abvm, 'sb_core_saved', side_effect=AssertionError('stop waited for the core copy')), \
                mock.patch('builtins.print'):
            abvm.sb_stop('sb1')
        return log

    def test_quit_gets_a_short_timeout(self):
        log = self.stop({'pid': 7, 'port': 5001, 'forward': 9}, 'quit')
        self.assertLess(log.index(f'timeout {abvm.QUIT_WAIT_S}'), log.index('quit'))
        self.assertLessEqual(abvm.QUIT_WAIT_S, 5)
        self.assertIn('close', log)
        self.assertFalse([c for c in log if str(c).startswith('kill')])

    def test_a_quit_that_times_out_falls_through_to_term(self):
        log = self.stop({'pid': 7, 'port': 5001, 'forward': 9}, 'TERM', quit_raises=TimeoutError('timed out'))
        self.assertIn('kill -TERM 7', log)
        self.assertIn('close', log)

    def test_a_hung_launcher_is_killed_after_term(self):
        log = self.stop({'pid': 7, 'port': 5001, 'forward': 9}, 'KILL')
        self.assertLess(log.index('kill -TERM 7'), log.index('kill -KILL 7'))

    def test_valgrind_goes_to_term_at_once(self):
        log = self.stop({'pid': 7, 'port': 5001, 'forward': 9, 'valgrind': True}, 'TERM')
        self.assertNotIn('driver', log)
        self.assertNotIn('quit', log)
        self.assertEqual(log[0], 'kill -TERM 7')

    def test_valgrind_that_ignores_term_is_killed_after_ten_seconds(self):
        log = self.stop({'pid': 7, 'port': 5001, 'forward': 9, 'valgrind': True}, 'KILL')
        self.assertEqual(log.count('sleep'), 100)
        self.assertLess(log.index('kill -TERM 7'), log.index('kill -KILL 7'))

    def test_stop_never_waits_on_the_core_copy(self):
        # sb_core_saved raises if called (see stop()); a core-saving mark on the share must not matter either
        self.stop({'pid': 7, 'port': 5001, 'forward': 9, 'valgrind': True}, 'TERM')
        self.stop({'pid': 7, 'port': 5001}, 'TERM')


class StaleCore(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.host = self.tmp.name
        self.ab = os.path.join(self.host, 'sb1', '.abvm')
        os.makedirs(self.ab)
        self.core = os.path.join(self.ab, 'core')

    def test_run_script_fails_loudly_when_core_cannot_be_made(self):
        t = abvm.RUN_SCRIPT_TEXT
        guard = t.index('[ ! -d "$ab/core" ]')
        self.assertLess(guard, t.index('cp -f "$c" "$ab/core/"'))
        self.assertIn('cannot make $ab/core', t)
        self.assertIn('>&2', t[guard:guard + 300])

    @unittest.skipUnless(sys.platform.startswith('linux'), 'runs the run script under sh')
    def test_run_script_with_a_file_for_core_says_so_and_keeps_the_exit_record(self):
        import subprocess
        bindir = os.path.join(self.host, 'bin')
        local = os.path.join(self.host, 'local')
        os.makedirs(bindir)
        os.makedirs(local)
        write(os.path.join(bindir, 'autobleem-gui'), '#!/bin/sh\nexit 4\n')
        os.chmod(os.path.join(bindir, 'autobleem-gui'), 0o755)
        write(self.core, 'stale')
        script = os.path.join(self.host, 'run.sh')
        write(script, abvm.RUN_SCRIPT_TEXT)
        env = dict(os.environ, ABVM_CORE_LOCAL=local, PATH='/usr/bin:/bin')
        r = subprocess.run(['sh', script, os.path.join(self.host, 'sb1')], cwd=bindir, env=env, capture_output=True,
                           text=True, timeout=60)
        self.assertEqual(r.returncode, 4)
        self.assertIn('cannot make', r.stderr)
        self.assertEqual(abvm.parse_exit_record(read(os.path.join(self.ab, 'exit-status')))['launcher_exit'], 4)
        self.assertFalse(os.path.exists(os.path.join(self.ab, abvm.CORE_SAVING)))

    def test_clear_run_record_removes_a_core_that_is_a_file(self):
        write(self.core, 'stale')
        with mock.patch.object(abvm, 'SB_HOST', self.host):
            abvm.sb_clear_run_record('sb1')
        self.assertFalse(os.path.lexists(self.core))

    def test_clear_run_record_still_removes_a_core_folder(self):
        write(os.path.join(self.core, 'core.1'), 'x')
        with mock.patch.object(abvm, 'SB_HOST', self.host):
            abvm.sb_clear_run_record('sb1')
        self.assertFalse(os.path.lexists(self.core))

    def test_a_core_that_cannot_be_removed_is_a_failure(self):
        write(self.core, 'stale')
        with mock.patch.object(abvm, 'SB_HOST', self.host), mock.patch.object(abvm.os, 'remove', side_effect=PermissionError('no')):
            with self.assertRaises(abvm.Fail):
                abvm.sb_drop_stale_core('sb1')

    def test_new_sandbox_from_a_template_with_a_stale_core_file_has_none(self):
        os.makedirs(os.path.join(self.host, '_template', '.abvm'))
        write(os.path.join(self.host, '_template', '.abvm', 'core'), 'stale')
        with mock.patch.object(abvm, 'SB_HOST', self.host), mock.patch.object(abvm, 'sb_open_modes', lambda p: None), \
                mock.patch.object(abvm.subprocess, 'run', side_effect=cp_r), mock.patch('builtins.print'):
            abvm.sb_new('sb2')
        self.assertFalse(os.path.lexists(os.path.join(self.host, 'sb2', '.abvm', 'core')))

    def test_relay_removes_a_stale_core_file_left_in_the_old_slot(self):
        write(self.core, 'stale')
        os.makedirs(os.path.join(self.host, '_template'))
        with mock.patch.object(abvm, 'SB_HOST', self.host), mock.patch.object(abvm, 'sb_open_modes', lambda p: None), \
                mock.patch.object(abvm, 'sb_stop', lambda n: None), \
                mock.patch.object(abvm, 'sb_remove', lambda n: shutil.rmtree(abvm.sb_host(n))), \
                mock.patch.object(abvm.subprocess, 'run', side_effect=cp_r), mock.patch('builtins.print'):
            abvm.sb_relay('sb1')
        self.assertFalse(os.path.lexists(self.core))


if __name__ == '__main__':
    unittest.main()
