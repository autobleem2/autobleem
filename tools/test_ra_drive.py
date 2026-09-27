#!/usr/bin/env python3
"""Offline unit tests for tools/ra_drive.py (R22) - a fake UDP server standing in for RetroArch's own
command_network_poll/command_parse_msg (command.c), so these cover the wire protocol, the log-tail parser
and the script parser without a real RetroArch binary anywhere on this machine (the owner's rule: no
RetroArch on this PC). Run with:

    python tools/test_ra_drive.py
    python -m unittest tools.test_ra_drive          (from the repo root)

Everything that needs a live RetroArch on the Debian machine (Xvfb, `start`, real screenshots, real log
lines) is out of scope here - see E:\\Programming\\_team\\r-items\\R22-TEST.md for the on-laptop steps.
"""
import os
import shutil
import socket
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ra_drive as rd  # noqa: E402


class FakeRetroArch:
    """A minimal stand-in for command_network_new()/command_network_poll() (command.c): one UDP socket,
    map[] commands (MENU_*, QUIT, SCREENSHOT, ...) get no reply at all - exactly like the real server,
    which never calls cmd->replier for those - and action_map[] commands (VERSION, GET_STATUS, ...) get
    the same reply text command.c builds, confirmed against RetroArch v1.22.2 source this session."""

    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('127.0.0.1', 0))
        self.port = self.sock.getsockname()[1]
        self.received = []
        self.status_state = 'CONTENTLESS'
        self._running = True
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def _loop(self):
        self.sock.settimeout(0.2)
        while self._running:
            try:
                data, addr = self.sock.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError:
                return
            line = data.decode('utf-8')
            self.received.append(line)
            self._respond(line, addr)

    def _respond(self, line, addr):
        name, _, arg = line.partition(' ')
        if name == 'VERSION':
            self.sock.sendto(b'1.22.2\n', addr)
        elif name == 'GET_STATUS':
            if self.status_state == 'CONTENTLESS':
                self.sock.sendto(b'GET_STATUS CONTENTLESS', addr)
            else:
                reply = 'GET_STATUS {} psx,game.bin,crc32=deadbeef\n'.format(self.status_state)
                self.sock.sendto(reply.encode('utf-8'), addr)
        elif name == 'GET_CONFIG_PARAM':
            self.sock.sendto('GET_CONFIG_PARAM {} somevalue'.format(arg).encode('utf-8'), addr)
        elif name == 'LOAD_STATE_SLOT':
            self.sock.sendto('LOAD_STATE_SLOT {}'.format(arg).encode('utf-8'), addr)
        elif name in ('SAVE_FILES', 'LOAD_FILES'):
            self.sock.sendto(b'OK\n', addr)
        # every NO_ARG_COMMANDS name (MENU_UP, QUIT, SCREENSHOT, ...): no reply, matching command.c.

    def stop(self):
        self._running = False
        self._thread.join(timeout=2)
        self.sock.close()


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        self.server = FakeRetroArch()
        self.client = rd.RaClient('127.0.0.1', self.server.port, timeout=1.0)

    def tearDown(self):
        self.client.close()
        self.server.stop()

    def test_no_arg_command_is_fire_and_forget(self):
        # QUIT is a map[] command: command() must return None and must not block waiting for a reply that
        # command.c's network_command_reply() never sends for it.
        self.assertIsNone(self.client.command('quit'))
        time.sleep(0.1)
        self.assertIn('QUIT', self.server.received)

    def test_unknown_command_rejected_locally(self):
        with self.assertRaises(rd.RaError):
            self.client.command('NOT_A_REAL_COMMAND')
        # never sent - checked before the packet went out, like command_verify() in command.c
        self.assertEqual(self.server.received, [])

    def test_reply_command_with_argument(self):
        self.assertEqual(self.client.command('load_state_slot', '2'), 'LOAD_STATE_SLOT 2')

    def test_no_arg_command_rejects_an_argument(self):
        with self.assertRaises(rd.RaError):
            self.client.command('QUIT', 'nonsense')

    def test_version(self):
        self.assertEqual(self.client.version(), '1.22.2')

    def test_press_maps_known_buttons(self):
        self.client.press('Up')
        self.client.press('a')
        self.client.press('TOGGLE')
        time.sleep(0.1)
        self.assertEqual(self.server.received, ['MENU_UP', 'MENU_A', 'MENU_TOGGLE'])

    def test_press_rejects_unknown_button(self):
        with self.assertRaises(rd.RaError):
            self.client.press('circle')  # not a menu button - see the module docstring

    def test_get_status_contentless(self):
        self.assertEqual(self.client.get_status(), {'state': 'CONTENTLESS'})

    def test_get_status_playing(self):
        self.server.status_state = 'PLAYING'
        status = self.client.get_status()
        self.assertEqual(status['state'], 'PLAYING')
        self.assertEqual(status['system_id'], 'psx')
        self.assertEqual(status['content'], 'game.bin')
        self.assertEqual(status['crc32'], 'deadbeef')

    def test_wait_status_succeeds_once_state_changes(self):
        def flip():
            time.sleep(0.2)
            self.server.status_state = 'PAUSED'
        threading.Thread(target=flip, daemon=True).start()
        status = self.client.wait_status('PAUSED', timeout=3.0, poll=0.05)
        self.assertEqual(status['state'], 'PAUSED')

    def test_wait_status_times_out(self):
        with self.assertRaises(rd.RaError):
            self.client.wait_status('PLAYING', timeout=0.3, poll=0.05)


class CommandTableTests(unittest.TestCase):
    """Sanity checks on the two tables themselves - catches a future edit that duplicates a name into both
    (which would silently change how it is routed) or introduces a typo against the confirmed source."""

    def test_no_arg_and_reply_tables_are_disjoint(self):
        self.assertEqual(set(rd.NO_ARG_COMMANDS) & set(rd.REPLY_COMMANDS), set())

    def test_buttons_map_to_confirmed_no_arg_commands(self):
        for name in rd.BUTTONS.values():
            self.assertIn(name, rd.NO_ARG_COMMANDS)

    def test_known_names_spot_check(self):
        # a handful of the exact strings read from command.h this session (see the module docstring)
        for name in ('MENU_TOGGLE', 'QUIT', 'SCREENSHOT', 'PAUSE_TOGGLE', 'LOAD_STATE', 'MENU_A'):
            self.assertIn(name, rd.NO_ARG_COMMANDS)
        for name in ('VERSION', 'GET_STATUS', 'GET_CONFIG_PARAM', 'LOAD_STATE_SLOT', 'SAVE_FILES'):
            self.assertIn(name, rd.REPLY_COMMANDS)


class ScreenshotPollTests(unittest.TestCase):
    def setUp(self):
        self.server = FakeRetroArch()
        self.client = rd.RaClient('127.0.0.1', self.server.port, timeout=1.0)
        self.dir = tempfile.mkdtemp(prefix='ra_drive_shots_')

    def tearDown(self):
        self.client.close()
        self.server.stop()
        shutil.rmtree(self.dir, ignore_errors=True)

    def _write_after(self, name, delay, content=b'x'):
        def go():
            time.sleep(delay)
            with open(os.path.join(self.dir, name), 'wb') as f:
                f.write(content)
        threading.Thread(target=go, daemon=True).start()

    def test_screenshot_waits_for_a_new_file(self):
        # a pre-existing file must not satisfy the wait (SCREENSHOT has no reply, so "new" is all we have)
        with open(os.path.join(self.dir, 'old.png'), 'wb') as f:
            f.write(b'stale')
        self._write_after('new.png', 0.2)
        path = self.client.screenshot(self.dir, timeout=2.0, poll=0.02)
        self.assertEqual(os.path.basename(path), 'new.png')
        time.sleep(0.1)
        self.assertIn('SCREENSHOT', self.server.received)

    def test_screenshot_times_out_with_no_new_file(self):
        with self.assertRaises(rd.RaError):
            self.client.screenshot(self.dir, timeout=0.3, poll=0.05)

    def test_screenshot_picks_the_newest_of_several(self):
        # both must land inside the *same* poll tick, or the first one found ends the wait (screenshot()
        # returns on the first new file it sees - it does not wait to see if another follows); write them
        # back-to-back after one delay so a single poll iteration catches both together.
        def go():
            time.sleep(0.1)
            path_a = os.path.join(self.dir, 'a.png')
            path_b = os.path.join(self.dir, 'b.png')
            with open(path_a, 'wb') as f:
                f.write(b'a')
            with open(path_b, 'wb') as f:
                f.write(b'bb')
            # force a deterministic mtime order regardless of filesystem timestamp resolution/tie-breaking
            now = time.time()
            os.utime(path_a, (now, now))
            os.utime(path_b, (now + 5, now + 5))
        threading.Thread(target=go, daemon=True).start()
        path = self.client.screenshot(self.dir, timeout=2.0, poll=0.3)
        self.assertEqual(os.path.basename(path), 'b.png')


class HashTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix='ra_drive_hash_')

    def tearDown(self):
        shutil.rmtree(self.dir, ignore_errors=True)

    def _solid(self, name, color):
        from PIL import Image
        path = os.path.join(self.dir, name)
        Image.new('RGB', (64, 64), color).save(path)
        return path

    def _checkerboard(self, name):
        from PIL import Image
        path = os.path.join(self.dir, name)
        img = Image.new('RGB', (64, 64))
        px = img.load()
        for y in range(64):
            for x in range(64):
                px[x, y] = (255, 255, 255) if (x // 8 + y // 8) % 2 else (0, 0, 0)
        img.save(path)
        return path

    def test_identical_images_hash_to_zero_distance(self):
        a = self._solid('a.png', (10, 20, 30))
        b = self._solid('b.png', (10, 20, 30))
        self.assertEqual(rd.hash_distance(rd.average_hash(a), rd.average_hash(b)), 0)

    def test_different_images_hash_far_apart(self):
        solid = self._solid('solid.png', (255, 255, 255))
        checker = self._checkerboard('checker.png')
        distance = rd.hash_distance(rd.average_hash(solid), rd.average_hash(checker))
        self.assertGreater(distance, 6)

    def test_wait_shot_matches_once_the_right_shot_appears(self):
        server = FakeRetroArch()
        client = rd.RaClient('127.0.0.1', server.port, timeout=1.0)
        try:
            ref = self._solid('ref.png', (200, 200, 200))
            wrong = self._checkerboard('wrong.png')

            def go():
                time.sleep(0.05)
                shutil.copy(wrong, os.path.join(self.dir, 'shot1.png'))
                time.sleep(0.15)
                shutil.copy(ref, os.path.join(self.dir, 'shot2.png'))
            threading.Thread(target=go, daemon=True).start()
            found = client.wait_shot(ref, self.dir, timeout=2.0, threshold=4, poll=0.03)
            self.assertEqual(os.path.basename(found), 'shot2.png')
        finally:
            client.close()
            server.stop()


class LogTailTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix='ra_drive_log_')
        self.path = os.path.join(self.dir, 'retroarch.log')

    def tearDown(self):
        shutil.rmtree(self.dir, ignore_errors=True)

    def test_finds_a_line_appended_after_it_starts_watching(self):
        with open(self.path, 'w', encoding='utf-8') as f:
            f.write('[INFO] Version of libretro API: 1.0\n')
        tail = rd.LogTail(self.path)  # starts at end of file - only new lines count

        def go():
            time.sleep(0.1)
            with open(self.path, 'a', encoding='utf-8') as f:
                f.write('[INFO] Redirecting save file to "/tmp/save.srm".\n')
        threading.Thread(target=go, daemon=True).start()
        line = tail.wait_for(r'Redirecting save file', timeout=2.0, poll=0.02)
        self.assertIn('Redirecting save file', line)

    def test_ignores_lines_already_present_before_it_starts(self):
        with open(self.path, 'w', encoding='utf-8') as f:
            f.write('[INFO] already here\n')
        tail = rd.LogTail(self.path)
        with self.assertRaises(rd.RaError):
            tail.wait_for(r'already here', timeout=0.3, poll=0.05)

    def test_from_start_sees_pre_existing_lines(self):
        with open(self.path, 'w', encoding='utf-8') as f:
            f.write('[INFO] already here\n')
        tail = rd.LogTail(self.path, from_start=True)
        line = tail.wait_for(r'already here', timeout=0.5, poll=0.05)
        self.assertIn('already here', line)

    def test_missing_file_is_a_timeout_not_a_crash(self):
        tail = rd.LogTail(os.path.join(self.dir, 'does-not-exist.log'))
        with self.assertRaises(rd.RaError):
            tail.wait_for(r'anything', timeout=0.2, poll=0.05)


class RunScriptTests(unittest.TestCase):
    def setUp(self):
        self.server = FakeRetroArch()
        self.client = rd.RaClient('127.0.0.1', self.server.port, timeout=1.0)
        self.shots = tempfile.mkdtemp(prefix='ra_drive_script_shots_')
        self.work = tempfile.mkdtemp(prefix='ra_drive_script_work_')

    def tearDown(self):
        self.client.close()
        self.server.stop()
        shutil.rmtree(self.shots, ignore_errors=True)
        shutil.rmtree(self.work, ignore_errors=True)

    def test_press_wait_and_bare_command(self):
        out = rd.run_script(self.client, 'press up; wait 10; QUIT')
        self.assertEqual(out, ['ok press up', 'ok wait', 'ok'])
        time.sleep(0.1)
        self.assertEqual(self.server.received, ['MENU_UP', 'QUIT'])

    def test_cmd_prefix_with_argument(self):
        out = rd.run_script(self.client, 'cmd LOAD_STATE_SLOT 3')
        self.assertEqual(out, ['LOAD_STATE_SLOT 3'])

    def test_menu_opens_and_confirms(self):
        rd.run_script(self.client, 'menu 2')
        time.sleep(0.1)
        self.assertEqual(self.server.received, ['MENU_TOGGLE', 'MENU_DOWN', 'MENU_DOWN', 'MENU_A'])

    def test_wait_status_inline(self):
        self.server.status_state = 'PLAYING'
        out = rd.run_script(self.client, 'wait_status PLAYING 2')
        self.assertIn("'state': 'PLAYING'", out[0])

    def test_shot_needs_shots_dir(self):
        with self.assertRaises(rd.RaError):
            rd.run_script(self.client, 'shot out.png')

    def test_shot_copies_the_new_file(self):
        def go():
            time.sleep(0.1)
            with open(os.path.join(self.shots, 'x.png'), 'wb') as f:
                f.write(b'data')
        threading.Thread(target=go, daemon=True).start()
        dest = os.path.join(self.work, 'copy.png')
        out = rd.run_script(self.client, 'shot ' + dest, shots=self.shots)
        self.assertEqual(out, ['ok ' + dest])
        self.assertTrue(os.path.exists(dest))

    def test_wait_log_needs_log(self):
        with self.assertRaises(rd.RaError):
            rd.run_script(self.client, 'wait_log something')

    def test_wait_log_with_explicit_timeout(self):
        log_path = os.path.join(self.work, 'retroarch.log')
        open(log_path, 'w', encoding='utf-8').close()
        tail = rd.LogTail(log_path)

        def go():
            time.sleep(0.05)
            with open(log_path, 'a', encoding='utf-8') as f:
                f.write('[INFO] Menu toggled\n')
        threading.Thread(target=go, daemon=True).start()
        out = rd.run_script(self.client, 'wait_log Menu toggled 2', log=tail)
        self.assertIn('Menu toggled', out[0])

    def test_unknown_bare_command_raises(self):
        with self.assertRaises(rd.RaError):
            rd.run_script(self.client, 'NOT_A_COMMAND')


class CfgTemplateTests(unittest.TestCase):
    """No RetroArch process involved - just checks the template this session's source reading says is
    required (network_cmd_enable/port, video gl, audio null, log_to_file + log_dir + timestamp off so the
    log lands at a predictable path, screenshot_directory) actually substitutes cleanly."""

    def test_required_keys_present(self):
        text = rd.CFG_TEMPLATE.format(port=55355, log_dir='/tmp/work', screenshots='/tmp/work/screenshots')
        for key in ('network_cmd_enable', 'network_cmd_port = "55355"', 'video_driver = "gl"',
                    'audio_driver = "null"', 'log_to_file', 'log_to_file_timestamp = "false"',
                    'log_dir = "/tmp/work"', 'screenshot_directory = "/tmp/work/screenshots"'):
            self.assertIn(key, text)


if __name__ == '__main__':
    unittest.main()
