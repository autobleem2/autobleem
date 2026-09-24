#!/usr/bin/env python3
"""A test source for the AutoBleem Store, served from this machine's LAN address.

    python tools/serve_test_store.py D:/AB/Games [--port 8124] [--name "My games"] [--no-sha]

Every folder under GAMES_DIR (not the ! ones) that holds disc images becomes one ps1 item named after the
folder, its images (.chd/.pbp/.img, else the .cue files) in name order as discs 1, 2, ... (.bin/.sbi/.ecm ride
along with no disc number). The
TSV is served at http://<lan ip>:<port>/store.tsv - add that URL in the Store's Sources tab, or put it in
System/Extensions/store/sources.txt on the client. A folder's serial comes from its Game.ini (a folder AutoBleem
has scanned has one): the Store finds the cover by it in its own covers databases. The games are served read-only, with Range requests (the
Store resumes a stopped download); nothing is written to GAMES_DIR. The sha256 of each file is worked out
once and kept in a cache file next to this machine's temp directory, keyed by path + size.

For testing on a LAN only: the server answers anyone who can reach the port. Stop it with Ctrl+C.
"""
import argparse
import hashlib
import http.server
import json
import os
import socket
import sys
import tempfile
import urllib.parse

IMAGES = ('.chd', '.pbp', '.img')
COMPANIONS = ('.cue', '.bin', '.sbi', '.ecm')


def lan_address(probe):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((probe, 9))  # no packet is sent: this only picks the interface that route would use
        return s.getsockname()[0]
    finally:
        s.close()


def game_ini_serial(folder):
    # the serial AutoBleem's scan wrote into the folder's Game.ini - what the Store finds the cover by
    ini = os.path.join(folder, 'Game.ini')
    if os.path.isfile(ini):
        with open(ini, encoding='utf-8', errors='replace') as f:
            for line in f:
                key, _, value = line.partition('=')
                if key.strip().lower() == 'serial':
                    return value.strip()
    return ''


def sha256_of(path, cache):
    key = '%s|%d' % (path, os.path.getsize(path))
    if key not in cache:
        h = hashlib.sha256()
        with open(path, 'rb') as f:
            for block in iter(lambda: f.read(1 << 20), b''):
                h.update(block)
        cache[key] = h.hexdigest()
    return cache[key]


def build_tsv(games_dir, base_url, name, with_sha):
    cache_file = os.path.join(tempfile.gettempdir(), 'autobleem-test-store-sha.json')
    cache = {}
    if with_sha and os.path.exists(cache_file):
        with open(cache_file, encoding='utf-8') as f:
            cache = json.load(f)
    lines = ['# autobleem-store 1', '# name: ' + name,
             '\t'.join(['kind', 'title', 'url', 'size', 'sha256', 'disc', 'name', 'serial'])]
    items = 0
    for folder in sorted(os.listdir(games_dir), key=str.lower):
        path = os.path.join(games_dir, folder)
        if folder.startswith('!') or not os.path.isdir(path):
            continue
        files = sorted(os.listdir(path), key=str.lower)
        images = [f for f in files if f.lower().endswith(IMAGES)]
        if images:  # a folder with chd/pbp as well as a cue/bin copy offers the images only
            companions = [f for f in files if f.lower().endswith('.sbi')]
        else:
            images = [f for f in files if f.lower().endswith('.cue')]
            companions = [f for f in files if f.lower().endswith(('.bin', '.sbi', '.ecm'))]
        if not images:
            continue
        serial = game_ini_serial(path)
        title = folder.replace('\t', ' ')
        entries = [(f, i + 1) for i, f in enumerate(images)] + [(f, 0) for f in companions]
        for file_name, disc in entries:
            full = os.path.join(path, file_name)
            sha = ''
            if with_sha:
                known = len(cache)
                sha = sha256_of(full, cache)
                if len(cache) != known:  # kept after every file, so a stopped start loses nothing
                    with open(cache_file, 'w', encoding='utf-8') as f:
                        json.dump(cache, f)
            print('  %s / %s' % (folder, file_name), file=sys.stderr)
            lines.append('\t'.join(['ps1', title, base_url + urllib.parse.quote(folder + '/' + file_name),
                                    str(os.path.getsize(full)), sha, str(disc) if disc else '', file_name,
                                    serial]))
            serial = ''  # once per item is enough
        items += 1
    return '\n'.join(lines) + '\n', items


def handler_for(games_dir, tsv):
    tsv_bytes = tsv.encode('utf-8')
    root = os.path.realpath(games_dir)

    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def do_HEAD(self):
            self.answer(False)

        def do_GET(self):
            self.answer(True)

        def answer(self, body):
            path = urllib.parse.unquote(urllib.parse.urlsplit(self.path).path)
            if path == '/store.tsv':
                self.send(200, 'text/tab-separated-values; charset=utf-8', len(tsv_bytes), body and tsv_bytes)
                return
            full = os.path.realpath(os.path.join(root, path.lstrip('/')))
            if not full.startswith(root + os.sep) or not os.path.isfile(full) or os.path.basename(
                    os.path.dirname(full)).startswith('!'):
                self.send(404, 'text/plain', 9, body and b'not found')
                return
            size = os.path.getsize(full)
            start, end, status = 0, size - 1, 200
            wanted = self.headers.get('Range', '')
            if wanted.startswith('bytes=') and ',' not in wanted:
                first, _, last = wanted[6:].partition('-')
                try:
                    start = int(first) if first else max(0, size - int(last))
                    end = min(int(last), size - 1) if first and last else size - 1
                except ValueError:
                    start, end = 0, size - 1
                if start >= size:
                    self.send_response(416)
                    self.send_header('Content-Range', 'bytes */%d' % size)
                    self.send_header('Content-Length', '0')
                    self.end_headers()
                    return
                status = 206
            self.send_response(status)
            self.send_header('Content-Type', 'application/octet-stream')
            self.send_header('Accept-Ranges', 'bytes')
            self.send_header('Content-Length', str(end - start + 1))
            if status == 206:
                self.send_header('Content-Range', 'bytes %d-%d/%d' % (start, end, size))
            self.end_headers()
            if not body:
                return
            with open(full, 'rb') as f:
                f.seek(start)
                left = end - start + 1
                try:
                    while left > 0:
                        block = f.read(min(left, 1 << 20))
                        if not block:
                            break
                        self.wfile.write(block)
                        left -= len(block)
                except (ConnectionResetError, BrokenPipeError, ConnectionAbortedError):
                    pass  # the client stopped (a pause or a cancel in the Store)

        def send(self, status, kind, length, data):
            self.send_response(status)
            self.send_header('Content-Type', kind)
            self.send_header('Content-Length', str(length))
            self.end_headers()
            if data:
                self.wfile.write(data)

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('games_dir')
    parser.add_argument('--port', type=int, default=8124)
    parser.add_argument('--name', default='My games (test)')
    parser.add_argument('--address', help="the address clients use (default: this machine's LAN address)")
    parser.add_argument('--probe', default='192.168.1.1', help='an address on the LAN, to find the interface')
    parser.add_argument('--no-sha', action='store_true', help='leave sha256 out (no hashing at start)')
    args = parser.parse_args()
    address = args.address or lan_address(args.probe)
    base_url = 'http://%s:%d/' % (address, args.port)
    tsv, items = build_tsv(args.games_dir, base_url, args.name, not args.no_sha)
    print('%d games; the source is %sstore.tsv' % (items, base_url), file=sys.stderr)
    server = http.server.ThreadingHTTPServer(('0.0.0.0', args.port), handler_for(args.games_dir, tsv))
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
