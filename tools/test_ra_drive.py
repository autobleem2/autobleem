#!/usr/bin/env python3
"""Offline unit tests for tools/ra_drive.py (R22) - a fake UDP server standing in for RetroArch's own
command_network_poll/command_parse_msg (command.c), so these cover the wire protocol, the log-tail parser
and the script parser without a real RetroArch binary anywhere on this machine (the owner's rule: no
RetroArch on this PC). Run with:

    python tools/test_ra_drive.py
    python -m unittest tools.test_ra_drive          (from the repo root)

Everything that needs a live RetroArch on the Debian test machine (Xvfb, `start`, real screenshots, real
log lines) is out of scope here - it is exercised on that machine directly, not by this offline suite.
"""
import inspect
import os
import shutil
import socket
import struct
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

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


class NoResponseTests(unittest.TestCase):
    """R22-RESULT.md step 3b (Nina): with nothing listening, request() used to leak a raw
    ConnectionResetError/OSError traceback instead of raising RaError. Two real shapes, both covered
    without relying on a specific OS's ICMP/RST behaviour (which the closed-port test can't pin down
    portably), plus a deterministic mock of each exact exception RetroArch's absence can produce."""

    def test_silent_server_times_out_with_a_clear_ra_error(self):
        # a bound-but-never-answering "server": nothing ever calls sendto back, so recvfrom just times out
        # - the common shape when RetroArch simply is not running (or a firewall silently drops it).
        silent = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        silent.bind(('127.0.0.1', 0))
        port = silent.getsockname()[1]
        client = rd.RaClient('127.0.0.1', port, timeout=0.3)
        try:
            with self.assertRaises(rd.RaError) as ctx:
                client.request('VERSION')
            message = str(ctx.exception)
            self.assertIn('no response from 127.0.0.1:{}'.format(port), message)
            self.assertIn('0.3', message)
            self.assertIn('RetroArch running', message)
        finally:
            client.close()
            silent.close()

    def test_closed_port_raises_a_clear_ra_error_not_a_traceback(self):
        # nothing listening at all. Real OS behaviour here varies (a plain timeout, or an OS-level
        # connection-refused error surfacing on the *next* socket call - Windows: WSAECONNRESET via
        # ConnectionResetError, Linux: typically ECONNREFUSED via ConnectionRefusedError) - this only
        # checks that whichever shape this machine produces, it comes out as one clear RaError, never a
        # raw socket traceback. The exact wording of each shape is pinned down deterministically below.
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
        probe.close()  # freed again - nothing is listening on it now
        client = rd.RaClient('127.0.0.1', port, timeout=0.5)
        try:
            with self.assertRaises(rd.RaError) as ctx:
                client.request('VERSION')
            message = str(ctx.exception)
            self.assertIn('127.0.0.1:{}'.format(port), message)
            self.assertIn('RetroArch running', message)
        finally:
            client.close()

    def _client_with_fake_socket(self, recv_exception):
        # socket.socket's methods are C-level slots (read-only on the real object - mock.patch.object on
        # them raises AttributeError), so the deterministic tests swap in a plain MagicMock standing in for
        # self.sock instead of patching the real socket's attributes.
        client = rd.RaClient('127.0.0.1', 12345, timeout=1.0)
        client.sock.close()  # the real socket RaClient's constructor opened - no longer needed
        client.sock = mock.MagicMock()
        client.sock.recvfrom.side_effect = recv_exception
        return client

    def test_connection_reset_is_mapped_to_refused(self):
        # deterministic: what Windows reports for a UDP send to a closed port (WSAECONNRESET on the recv).
        client = self._client_with_fake_socket(ConnectionResetError('WSAECONNRESET'))
        try:
            with self.assertRaises(rd.RaError) as ctx:
                client.request('VERSION')
            message = str(ctx.exception)
            self.assertIn('127.0.0.1:12345 refused', message)
            self.assertIn('RetroArch running', message)
        finally:
            client.close()

    def test_connection_refused_is_mapped_to_refused(self):
        # deterministic: the usual Linux shape (ECONNREFUSED).
        client = self._client_with_fake_socket(ConnectionRefusedError('ECONNREFUSED'))
        try:
            with self.assertRaises(rd.RaError) as ctx:
                client.request('VERSION')
            message = str(ctx.exception)
            self.assertIn('127.0.0.1:12345 refused', message)
        finally:
            client.close()

    def test_wait_status_survives_a_single_failed_poll(self):
        # get_status() now raises RaError (not socket.timeout/OSError) on a no-response poll; wait_status
        # must still treat one failed attempt as "not yet", not let it end the whole wait early.
        server = FakeRetroArch()
        client = rd.RaClient('127.0.0.1', server.port, timeout=0.2)
        try:
            def flip():
                time.sleep(0.15)
                server.status_state = 'PLAYING'
            threading.Thread(target=flip, daemon=True).start()
            status = client.wait_status('PLAYING', timeout=2.0, poll=0.05)
            self.assertEqual(status['state'], 'PLAYING')
        finally:
            client.close()
            server.stop()

    def test_main_reports_no_response_and_exits_1(self):
        silent = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        silent.bind(('127.0.0.1', 0))
        port = silent.getsockname()[1]
        try:
            rc = rd.main(['ra_drive.py', 'VERSION', '--host', '127.0.0.1', '--port', str(port)])
            self.assertEqual(rc, 1)
        finally:
            silent.close()


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


class CmdStartReadinessPollTests(unittest.TestCase):
    """No RetroArch process involved (cmd_start needs Xvfb, not run offline) - a source-level regression
    guard for the R22 phase 2 finding: cmd_start's own readiness poll must never call GET_STATUS. RetroArch
    1.22.2's official Linux x86_64 build reproducibly segfaults the instant GET_STATUS is answered while any
    core is actively running (PLAYING) - gdb-confirmed with three unrelated cores (2048, mrboom, a from-
    scratch NES ROM under fceumm), same crash address inside RetroArch's own binary every time, independent
    of contentless vs real ROM content. See R22-phase2-report.md. VERSION is what cmd_start must poll with
    instead - it answers just as well for "is the command port up yet" and never touches that code path."""

    def test_readiness_poll_uses_version_not_get_status(self):
        source = inspect.getsource(rd.cmd_start)
        self.assertIn("command('VERSION')", source)
        self.assertNotIn('get_status()', source)

    def test_start_records_display_and_xauth_for_shot_display(self):
        # R22, this session (option (b)): a source-level regression guard, same shape as the test above -
        # cmd_start must keep writing ra_drive.display (parsed from Xvfb's own diagnostic output - xvfb-run
        # hardcodes fd 3 for itself and closes it before exec'ing the wrapped command, so `-displayfd 1` +
        # `-e diag_path` is what actually works, not a custom fd number) and pin ra_drive.Xauthority to a
        # known path, or shot_display has nothing to find later.
        source = inspect.getsource(rd.cmd_start)
        self.assertIn('ra_drive.display', source)
        self.assertIn('ra_drive.Xauthority', source)
        self.assertIn('-displayfd 1', source)


def _build_xwd(width, height, pixels, bits_per_pixel=24, byte_order=0, name=b''):
    """A minimal synthetic XWD file (X11/XWDFile.h, version 7, ZPixmap) for XwdParsingTests - no `xwd`
    binary or live X server involved. `pixels` is a flat list of (r, g, b) tuples, row-major, top row
    first. Masks are the common 0xFF0000/0xFF00/0xFF (red/green/blue), bytes_per_line = width *
    bytes_per_pixel exactly (no row padding) - matches what a real capture against Xvfb :N looked like,
    apart from the row padding some window managers/servers add (xwd_to_rgb_rows uses bytes_per_line from
    the header for the row stride either way, so padding is exercised separately, not needed for shape
    coverage here)."""
    bytes_per_pixel = bits_per_pixel // 8
    stride = width * bytes_per_pixel
    header_size = 100 + len(name) + (1 if name else 0)  # +1 for the trailing NUL xwd itself always writes
    header = struct.pack(
        '>25I',
        header_size, 7, rd._XWD_ZPIXMAP, bits_per_pixel, width, height, 0, byte_order, 32, 0, 8,
        bits_per_pixel, stride, 4, 0xFF0000, 0xFF00, 0xFF, 8, 0, 0, width, height, 0, 0, 0,
    )
    body = bytearray()
    for (r, g, b) in pixels:
        word = (r << 16) | (g << 8) | b
        body += word.to_bytes(bytes_per_pixel, 'little' if byte_order == 0 else 'big')
    return header + name + (b'\x00' if name else b'') + bytes(body)


class ParseXvfbDiagDisplayTests(unittest.TestCase):
    """_parse_xvfb_diag_display against the real shape of xvfb-run's -e file (R22, this session): the
    display number mixed in with unrelated startup noise (libEGL/DRM warnings), not always the first line -
    confirmed live on the Debian test machine, see R22-phase2-report.md section 11."""

    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix='ra_drive_diag_')
        self.path = os.path.join(self.dir, 'diag')

    def tearDown(self):
        shutil.rmtree(self.dir, ignore_errors=True)

    def _write(self, text):
        with open(self.path, 'w', encoding='utf-8') as f:
            f.write(text)

    def test_missing_file_returns_none(self):
        self.assertIsNone(rd._parse_xvfb_diag_display(self.path))

    def test_bare_number_only(self):
        self._write('100\n')
        self.assertEqual(rd._parse_xvfb_diag_display(self.path), '100')

    def test_number_after_dri_warnings_like_a_real_capture(self):
        self._write('libEGL warning: failed to open /dev/dri/card0: Permission denied\n\n'
                     'libEGL warning: failed to open /dev/dri/card0: Permission denied\n\n100\n')
        self.assertEqual(rd._parse_xvfb_diag_display(self.path), '100')

    def test_no_numeric_line_returns_none(self):
        self._write('libEGL warning: failed to open /dev/dri/card0: Permission denied\n')
        self.assertIsNone(rd._parse_xvfb_diag_display(self.path))

    def test_empty_file_returns_none(self):
        self._write('')
        self.assertIsNone(rd._parse_xvfb_diag_display(self.path))


class XwdParsingTests(unittest.TestCase):
    """Offline coverage for xwd_to_rgb_rows - the synthetic files here stand in for what `xwd -root` writes
    (proven correct against a real capture and a known-good screenshot in R22-phase2-report.md section 11);
    this class does not need `xwd`, Xvfb, or a live X server anywhere."""

    def test_24bpp_four_pixels_decode_correctly(self):
        pixels = [(10, 20, 30), (40, 50, 60), (70, 80, 90), (100, 110, 120)]
        data = _build_xwd(2, 2, pixels, bits_per_pixel=24)
        width, height, rows = rd.xwd_to_rgb_rows(data)
        self.assertEqual((width, height), (2, 2))
        self.assertEqual(rows, [[pixels[0], pixels[1]], [pixels[2], pixels[3]]])

    def test_32bpp_decodes_correctly(self):
        pixels = [(1, 2, 3), (250, 251, 252)]
        data = _build_xwd(2, 1, pixels, bits_per_pixel=32)
        width, height, rows = rd.xwd_to_rgb_rows(data)
        self.assertEqual((width, height), (2, 1))
        self.assertEqual(rows, [pixels])

    def test_black_and_white_extremes(self):
        pixels = [(0, 0, 0), (255, 255, 255)]
        data = _build_xwd(2, 1, pixels)
        _, _, rows = rd.xwd_to_rgb_rows(data)
        self.assertEqual(rows, [pixels])

    def test_window_name_is_skipped_correctly(self):
        pixels = [(9, 8, 7)]
        data = _build_xwd(1, 1, pixels, name=b'xterm')
        _, _, rows = rd.xwd_to_rgb_rows(data)
        self.assertEqual(rows, [[(9, 8, 7)]])

    def test_wrong_file_version_rejected(self):
        data = bytearray(_build_xwd(1, 1, [(1, 1, 1)]))
        struct.pack_into('>I', data, 4, 6)  # file_version is header field 2 (offset 4)
        with self.assertRaises(rd.RaError):
            rd.xwd_to_rgb_rows(bytes(data))

    def test_wrong_pixmap_format_rejected(self):
        data = bytearray(_build_xwd(1, 1, [(1, 1, 1)]))
        struct.pack_into('>I', data, 8, 1)  # pixmap_format is field 3 (offset 8) - 1 is XYPixmap, not 2
        with self.assertRaises(rd.RaError):
            rd.xwd_to_rgb_rows(bytes(data))

    def test_unsupported_bits_per_pixel_rejected(self):
        data = bytearray(_build_xwd(1, 1, [(1, 1, 1)]))
        struct.pack_into('>I', data, 44, 16)  # bits_per_pixel is field 12 (offset 44)
        with self.assertRaises(rd.RaError):
            rd.xwd_to_rgb_rows(bytes(data))

    def test_truncated_file_rejected(self):
        data = _build_xwd(4, 4, [(1, 2, 3)] * 16)
        with self.assertRaises(rd.RaError):
            rd.xwd_to_rgb_rows(data[:-10])

    def test_too_short_to_be_a_header_rejected(self):
        with self.assertRaises(rd.RaError):
            rd.xwd_to_rgb_rows(b'not an xwd file')


class CaptureDisplayTests(unittest.TestCase):
    """capture_display's own file-handling, offline - no `xwd` binary needed for these paths (they never
    reach the subprocess call)."""

    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix='ra_drive_capture_')

    def tearDown(self):
        shutil.rmtree(self.dir, ignore_errors=True)

    def test_missing_display_file_raises_clearly(self):
        with self.assertRaises(rd.RaError) as ctx:
            rd.capture_display(self.dir, os.path.join(self.dir, 'out.png'))
        self.assertIn('ra_drive.display', str(ctx.exception))

    def test_empty_display_file_raises_clearly(self):
        open(os.path.join(self.dir, 'ra_drive.display'), 'w', encoding='utf-8').close()
        with self.assertRaises(rd.RaError) as ctx:
            rd.capture_display(self.dir, os.path.join(self.dir, 'out.png'))
        self.assertIn('empty', str(ctx.exception))


class RunScriptShotDisplayTests(unittest.TestCase):
    """run_script's 'shot_display' dispatch - capture_display itself is mocked out (it needs `xwd`, covered
    against a live instance in R22-phase2-report.md section 11, not here)."""

    def setUp(self):
        self.server = FakeRetroArch()
        self.client = rd.RaClient('127.0.0.1', self.server.port, timeout=1.0)
        self.dir = tempfile.mkdtemp(prefix='ra_drive_shotdisplay_')

    def tearDown(self):
        self.client.close()
        self.server.stop()
        shutil.rmtree(self.dir, ignore_errors=True)

    def test_shot_display_needs_cfg_dir(self):
        with self.assertRaises(rd.RaError):
            rd.run_script(self.client, 'shot_display out.png')

    def test_shot_display_calls_capture_display_with_work_dir_and_dest(self):
        dest = os.path.join(self.dir, 'menu.png')
        with mock.patch.object(rd, 'capture_display') as m:
            out = rd.run_script(self.client, 'shot_display ' + dest, work_dir='/some/cfg/dir')
        m.assert_called_once_with('/some/cfg/dir', dest)
        self.assertEqual(out, ['ok ' + dest])

    def test_main_strips_cfg_from_the_run_script_text(self):
        # R22, this session: found live on the Debian machine - main() peeked --cfg for cfg_dir but never
        # removed it from args, so for `run` (and a bare command) it leaked into `' '.join(args)`, the
        # script text itself. shot_display's destination is "everything after the command name", so it
        # silently picked up "--cfg DIR" as part of the filename it asked xwd to write to ("xwd: error:
        # Can't open output file as specified" - a real failure, reproduced against a live instance before
        # this fix). `start` is the one exception: cmd_start does its own --cfg parsing straight out of
        # args, so main() must leave it there for that path specifically (not exercised here).
        dest = os.path.join(self.dir, 'menu.png')
        with mock.patch.object(rd, 'capture_display') as m:
            rc = rd.main(['ra_drive.py', 'run', 'shot_display ' + dest, '--cfg', self.dir,
                           '--port', str(self.server.port)])
        self.assertEqual(rc, 0)
        m.assert_called_once_with(os.path.abspath(self.dir), dest)


if __name__ == '__main__':
    unittest.main()
