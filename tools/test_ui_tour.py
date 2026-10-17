import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ui_tour  # noqa: E402


class FakeDriver:
    made = []

    def __init__(self, port, host=None, **kw):
        self.port, self.host = port, host
        self.closed = False
        FakeDriver.made.append(self)

    def cmd(self, line):
        return 'ok GuiLauncher'

    def run(self, script):
        pass

    def close(self):
        self.closed = True


class UiTourModeTest(unittest.TestCase):
    def setUp(self):
        FakeDriver.made = []
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


if __name__ == '__main__':
    unittest.main()
