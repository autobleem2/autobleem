#!/usr/bin/env python3
"""Drives padsim (R25) - the test-only virtual X360 gamepad in the pcusb-test VM's guest, reached over the
libvirt virtio-serial channel's host-side unix socket. Test-stick-only; never on a real image.

  python tools/padsim_client.py --socket /path/to/pcusb-test-padsim.sock press a
  python tools/padsim_client.py --socket ... run "dpad down; wait 200; dpad center; press a"

One command per line, one reply per line ("ok" or "err <msg>") - the same shape ab_drive.py's DebugDriver
uses, but padsim's own small vocabulary:
  press <btn>            release <btn>          hold <btn> <ms>
  stick <left|right> <x> <y>   (x,y in -32768..32767)
  trigger <l2|r2> <0..255>
  dpad <up|down|left|right|center>
Buttons: a b x y l1 r1 l2 r2 start select l3 r3

The socket path is whatever `virsh dumpxml pcusb-test` shows for the org.autobleem.padsim channel's
<source path='...'> - docs/pc-test-machine.md has the current one.
"""
import socket
import sys
import time


class PadsimClient:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(path)
        self.buf = b''

    def cmd(self, line):
        self.sock.sendall((line + '\n').encode('utf-8'))
        while b'\n' not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError('padsim closed the connection')
            self.buf += chunk
        reply, self.buf = self.buf.split(b'\n', 1)
        reply = reply.decode('utf-8', 'replace')
        if reply.startswith('err'):
            raise RuntimeError(f'{line!r}: {reply}')
        return reply

    def run_script(self, script):
        for part in script.split(';'):
            part = part.strip()
            if not part:
                continue
            if part.startswith('wait '):
                time.sleep(int(part.split()[1]) / 1000.0)
                continue
            self.cmd(part)


def main(argv):
    if '--socket' not in argv:
        print(__doc__)
        return 1
    i = argv.index('--socket')
    path = argv[i + 1]
    rest = argv[:i] + argv[i + 2:]
    if not rest:
        print(__doc__)
        return 1

    client = PadsimClient(path)
    if rest[0] == 'run':
        client.run_script(rest[1])
    else:
        print(client.cmd(' '.join(rest)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
