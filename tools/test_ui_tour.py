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
        apps = [s for s in FakeDriver.scripts if s.endswith('press x; wait_screen GuiLauncher; wait 800')]
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

    def with_screens(self, screens, sticky=None):
        """a tour whose `screen` answers come from the list; once it is used up, `sticky` (else GuiLauncher)"""
        queue = list(screens)

        class Scripted(FakeDriver):
            def cmd(self, line):
                if line == 'screen':
                    return 'ok ' + (queue.pop(0) if queue else (sticky or 'GuiLauncher'))
                return 'ok'

        out = io.StringIO()
        err = None
        with contextlib.redirect_stdout(out), mock.patch.object(ui_tour.ab_drive, 'Driver', Scripted), \
                mock.patch.object(ui_tour.ab_drive, 'sheet'):
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
        self.assertIsNone(self.with_screens(['GuiUpdatePrompt', 'GuiLauncher']))
        self.assertEqual(FakeDriver.scripts[0], 'press o; wait 400')
        self.assertEqual(FakeDriver.scripts[1], 'wait 800')

    def test_a_menu_is_closed_first(self):
        self.assertIsNone(self.with_screens(['GuiSystemMenu', 'GuiOptions', 'GuiLauncher']))
        self.assertEqual(FakeDriver.scripts[:2], ['press o; wait 400'] * 2)

    def test_home_never_reached_names_the_screen(self):
        err = self.with_screens([], sticky='GuiOptions')
        self.assertIn('GuiOptions', err)
        self.assertEqual(FakeDriver.scripts.count('press o; wait 400'), ui_tour.HOME_TRIES)
        self.assertTrue(os.path.exists(self.out))

    def test_no_games_is_a_clear_error(self):
        # home at the start, then Up opened the Quick menu; Circle closes it again
        err = self.with_screens(['GuiLauncher', 'GuiSystemMenu', 'GuiSystemMenu', 'GuiLauncher'])
        self.assertEqual(err, ui_tour.NO_GAMES)
        self.assertIn('games=30', err)
        self.assertEqual(FakeDriver.scripts[-1], 'press o; wait 400')

    def test_about_is_left_with_two_backs(self):
        self.summary()
        self.assertIn('press start; wait 300; press o; wait 500; press o; wait_screen GuiLauncher', FakeDriver.scripts)


if __name__ == '__main__':
    unittest.main()
