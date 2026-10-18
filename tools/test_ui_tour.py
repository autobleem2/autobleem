import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ui_tour  # noqa: E402


class FakeDriver:
    made = []
    scripts = []

    def __init__(self, port, host=None, **kw):
        self.port, self.host = port, host
        self.closed = False
        FakeDriver.made.append(self)

    def cmd(self, line):
        return 'ok GuiLauncher'

    def run(self, script):
        FakeDriver.scripts.append(script)

    def close(self):
        self.closed = True


class UiTourModeTest(unittest.TestCase):
    def setUp(self):
        FakeDriver.made = []
        FakeDriver.scripts = []
        self.tmp = tempfile.TemporaryDirectory()
        self.out = os.path.join(self.tmp.name, 'tour')

    def tearDown(self):
        self.tmp.cleanup()

    def tour(self, *extra):
        with mock.patch.object(ui_tour.ab_drive, 'Driver', FakeDriver), \
                mock.patch.object(ui_tour.ab_drive, 'sheet'), \
                mock.patch.object(ui_tour.subprocess, 'check_call') as start, \
                mock.patch.object(ui_tour.subprocess, 'call') as stop:
            rc = ui_tour.main(['ui_tour.py', self.out, '--port', '7801'] + list(extra))
        return rc, start, stop

    def test_attach_does_not_start_or_stop(self):
        rc, start, stop = self.tour('--attach')
        self.assertEqual(rc, 0)
        start.assert_not_called()
        stop.assert_not_called()
        self.assertEqual(FakeDriver.made[0].port, 7801)
        self.assertTrue(FakeDriver.made[0].closed)

    def test_attach_host(self):
        self.tour('--attach', '--host', '10.0.0.5')
        self.assertEqual(FakeDriver.made[0].host, '10.0.0.5')

    def test_default_starts_and_stops(self):
        rc, start, stop = self.tour()
        self.assertEqual(rc, 0)
        self.assertEqual(start.call_count, 1)
        self.assertEqual(stop.call_count, 1)
        self.assertEqual(start.call_args[0][0][-3:], ['start', '--port', '7801'])
        self.assertEqual(stop.call_args[0][0][-3:], ['stop', '--port', '7801'])
        self.assertEqual(FakeDriver.made[0].host, ui_tour.ab_drive.DEFAULT_HOST)

    def summary(self, **kw):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), mock.patch.object(ui_tour.ab_drive, 'Driver', FakeDriver), \
                mock.patch.object(ui_tour.ab_drive, 'sheet'):
            ui_tour.main(['ui_tour.py', self.out, '--attach'], **kw)
        return json.loads(out.getvalue().strip().splitlines()[-1])

    def test_with_cores_the_retroarch_screens_are_steps(self):
        self.assertEqual(self.summary(), {'skipped': []})
        ra = [s for s in FakeDriver.scripts if 'press r1; wait 200; press x' in s]
        self.assertEqual(len(ra), 2)  # the RetroArch set and the Apps set, one tab each
        self.assertTrue(any('press down; wait 400; press right; wait 300; press x; wait 800' == s
                            for s in FakeDriver.scripts))

    def test_without_cores_the_retroarch_screens_are_skipped_and_not_walked(self):
        got = self.summary(ra_cores=False)
        self.assertEqual(got, {'skipped': [{'screen': 'launcher-retroarch', 'reason': ui_tour.NO_RA_CORE},
                                           {'screen': 'game-editor-retroarch', 'reason': ui_tour.NO_RA_CORE}]})
        apps = [s for s in FakeDriver.scripts if s.endswith('press x; wait_screen GuiLauncher; wait_ready 10')]
        self.assertEqual(len(apps), 1)  # the Apps set only, two tabs to the right
        self.assertEqual(apps[0].count('press r1'), 2)
        self.assertFalse(any('press down; wait 400; press right; wait 300; press x; wait 800' == s
                             for s in FakeDriver.scripts))

    def test_flag_skips_too(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), mock.patch.object(ui_tour.ab_drive, 'Driver', FakeDriver), \
                mock.patch.object(ui_tour.ab_drive, 'sheet'):
            ui_tour.main(['ui_tour.py', self.out, '--attach', '--no-ra-core'])
        self.assertEqual(len(json.loads(out.getvalue().strip().splitlines()[-1])['skipped']), 2)

    def with_screens(self, screens, sticky=None, ready=(), busy=(), busy_sticky=0, timeout=0.1):
        """a tour whose `screen` answers come from the list; once it is used up, `sticky` (else GuiLauncher).
        ready / busy: the `screen` and `busy` answers of the wait before the first move (then GuiLauncher / 0 or
        busy_sticky); the list `screens` starts with the first move"""
        queue = list(screens)
        waiting = list(ready)
        spinner = list(busy)

        class Scripted(FakeDriver):
            moved = False

            def cmd(self, line):
                if line == 'screen':
                    if not Scripted.moved:
                        return 'ok ' + (waiting.pop(0) if waiting else 'GuiLauncher')
                    return 'ok ' + (queue.pop(0) if queue else (sticky or 'GuiLauncher'))
                if line == 'busy':
                    return 'ok %d' % (spinner.pop(0) if spinner else busy_sticky)
                return 'ok'

            def run(self, script):
                Scripted.moved = True
                super().run(script)

        out = io.StringIO()
        err = None
        with contextlib.redirect_stdout(out), mock.patch.object(ui_tour.ab_drive, 'Driver', Scripted), \
                mock.patch.object(ui_tour.ab_drive, 'sheet'), mock.patch.object(ui_tour, 'READY_POLL', 0.001), \
                mock.patch.object(ui_tour, 'READY_TIMEOUT', timeout):
            try:
                ui_tour.main(['ui_tour.py', self.out, '--attach'])
            except RuntimeError as e:
                err = str(e)
        return err

    def test_home_launcher_needs_no_presses(self):
        self.summary()
        self.assertNotIn('press o; wait 400', FakeDriver.scripts)
        self.assertEqual(FakeDriver.scripts[0], 'wait 800')

    def test_a_modal_is_closed_first(self):
        self.assertIsNone(self.with_screens(['GuiLauncher'], ready=['GuiUpdatePrompt'] * 1000))
        self.assertEqual(FakeDriver.scripts[0], 'press o; wait 400')
        self.assertEqual(FakeDriver.scripts[1], 'wait 800')

    def test_a_menu_is_closed_first(self):
        self.assertIsNone(self.with_screens(['GuiOptions', 'GuiLauncher'], ready=['GuiSystemMenu'] * 1000))
        self.assertEqual(FakeDriver.scripts[:2], ['press o; wait 400'] * 2)

    def test_home_never_reached_names_the_screen(self):
        err = self.with_screens([], sticky='GuiOptions', ready=['GuiOptions'] * 1000)
        self.assertIn('GuiOptions', err)
        self.assertEqual(FakeDriver.scripts.count('press o; wait 400'), ui_tour.HOME_TRIES)
        self.assertTrue(os.path.exists(self.out))

    def test_no_games_is_a_clear_error(self):
        # home at the start, then Up opened the Quick menu; Circle closes it again
        err = self.with_screens(['GuiLauncher', 'GuiLauncher', 'GuiSystemMenu', 'GuiSystemMenu', 'GuiLauncher'])
        self.assertTrue(err.startswith(ui_tour.NO_GAMES))
        self.assertIn('games=30', err)
        self.assertIn('now: GuiSystemMenu', err)
        self.assertIn('busy: 0', err)
        self.assertEqual(FakeDriver.scripts[-1], 'press o; wait 400')

    def test_a_busy_launcher_is_waited_for_then_the_tour_goes_on(self):
        err = self.with_screens([], ready=['GuiLauncher', 'GuiLauncher', 'GuiLauncher', 'GuiLauncher'],
                                busy=[1, 1, 0, 1, 0, 0])
        self.assertIsNone(err)
        self.assertEqual(FakeDriver.scripts[0], 'wait 800')    # the first move came after the wait

    def test_a_launcher_still_loading_is_waited_for(self):
        self.assertIsNone(self.with_screens([], ready=['GuiLoading', 'GuiLoading', 'GuiLauncher', 'GuiLauncher']))
        self.assertEqual(FakeDriver.scripts[0], 'wait 800')

    def test_a_launcher_busy_forever_times_out_naming_screen_and_busy(self):
        err = self.with_screens([], busy_sticky=1)
        self.assertIn('not ready', err)
        self.assertIn('now: GuiLauncher', err)
        self.assertIn('busy: 1', err)
        self.assertEqual(FakeDriver.scripts, [])               # nothing was pressed

    def test_a_modal_that_keeps_the_launcher_free_is_left_to_the_home_step(self):
        err = self.with_screens(['GuiLauncher'], ready=['GuiUpdatePrompt'] * 1000, sticky='GuiLauncher')
        self.assertIsNone(err)
        self.assertEqual(FakeDriver.scripts[0], 'press o; wait 400')

    def failing_tour(self, fail_on):
        """a tour whose script containing `fail_on` raises like a failed wait_screen; the screen is GuiOptions"""
        class Failing(FakeDriver):
            def cmd(self, line):
                if line == 'screen':
                    return 'ok GuiOptions' if Failing.failed else 'ok GuiLauncher'
                if line == 'selected':
                    return 'ok 3|Some Game'
                if line == 'busy':
                    return 'ok 0'
                return 'ok'

            def run(self, script):
                if fail_on in script:
                    Failing.failed = True
                    raise RuntimeError(f"step 6 'wait_screen GuiEditor' failed: screen GuiEditor did not show")
                super().run(script)

        Failing.failed = False
        shots = []
        with mock.patch.object(ui_tour.ab_drive, 'Driver', Failing), \
                mock.patch.object(ui_tour.ab_drive, 'sheet') as sheet, \
                contextlib.redirect_stdout(io.StringIO()):
            try:
                ui_tour.main(['ui_tour.py', self.out, '--attach'], shot_path=lambda p: shots.append(p) or p)
                err = None
            except RuntimeError as e:
                err = str(e)
        return err, shots, sheet

    def test_a_failed_step_leaves_a_shot_a_trace_and_a_sheet(self):
        err, shots, sheet = self.failing_tour('press x; wait_screen GuiEditor')
        self.assertIn('GuiEditor did not show', err)
        self.assertIn('now: GuiOptions, busy: 0, row: 3', err)
        self.assertIn('trace.json', err)
        with open(os.path.join(self.out, 'trace.json')) as f:
            done = len(json.load(f)['steps'])
        self.assertTrue(os.path.basename(shots[-1]).endswith('-fail-%d.png' % (done + 1)))
        sheet.assert_called_once()
        self.assertEqual(sheet.call_args[0][0], os.path.join(self.out, 'tour.png'))
        self.assertIn(shots[-1], sheet.call_args[0][1])      # the sheet holds the failing screen too

    def test_the_trace_lists_every_step_done_with_screen_busy_and_row(self):
        self.failing_tour('press x; wait_screen GuiEditor')
        with open(os.path.join(self.out, 'trace.json')) as f:
            record = json.load(f)
        self.assertEqual(record['now'], {'screen': 'GuiOptions', 'busy': 0, 'row': 3})
        self.assertIn('GuiEditor did not show', record['error'])
        steps = record['steps']
        self.assertEqual([s['step'] for s in steps], list(range(1, len(steps) + 1)))
        self.assertEqual(steps[0]['script'], 'wait 800')
        self.assertEqual(steps[0], {'step': 1, 'script': 'wait 800', 'screen': 'GuiLauncher', 'busy': 0, 'row': 3})
        self.assertFalse(any('press x; wait_screen GuiEditor' in s['script'] for s in steps))  # the failed one is not done
        self.assertTrue(any('menu about' in s['script'] for s in steps))

    def test_a_good_tour_writes_the_trace_too(self):
        self.summary()
        with open(os.path.join(self.out, 'trace.json')) as f:
            steps = json.load(f)['steps']
        self.assertGreater(len(steps), 20)
        self.assertEqual(steps[0]['screen'], 'GuiLauncher')
        self.assertIsNone(steps[0]['row'])   # a driver without `selected`

    def test_hardware_info_is_left_through_the_system_menu(self):
        self.summary()
        scripts = FakeDriver.scripts
        i = scripts.index('press o; wait_screen GuiSystemMenu; press o; wait_screen GuiLauncher; wait_ready 10')
        self.assertTrue(scripts[i - 1].startswith('press r2'))        # straight after the page-2 look
        self.assertTrue(scripts[i + 1].startswith('menu about'))
        self.assertNotIn('press o; wait_screen GuiLauncher', scripts[i - 3:i + 1])

    def test_the_game_editor_is_opened_by_moves_that_are_waited_for(self):
        self.summary()
        editor = [s for s in FakeDriver.scripts if 'wait_screen GuiEditor' in s]
        self.assertEqual(editor, ['press down; wait_ready 10; press right; wait_ready 10; press x; '
                                  'wait_screen GuiEditor; wait 200'])
        self.assertNotIn('wait 300; press x', editor[0])             # no fixed pause before Cross

    def test_about_is_left_with_two_backs(self):
        self.summary()
        self.assertIn('press start; wait 300; press o; wait 500; press o; wait_screen GuiLauncher; wait_ready 10',
                      FakeDriver.scripts)

    def test_every_return_to_the_launcher_is_followed_by_wait_ready(self):
        self.summary()
        returns = [s for s in FakeDriver.scripts if 'wait_screen GuiLauncher' in s]
        self.assertGreater(len(returns), 8)
        for s in returns:
            self.assertTrue(s.endswith('wait_screen GuiLauncher; wait_ready 10'), s)

    def test_the_error_text_keeps_now_busy_and_row(self):
        err, _, _ = self.failing_tour('press x; wait_screen GuiEditor')
        self.assertIn('[now: GuiOptions, busy: 0, row: 3;', err)


if __name__ == '__main__':
    unittest.main()
