#!/usr/bin/env python3
"""The Sony-hash guard: no Sony PlayStation BIOS image is tracked in this repository (run by ctest, or: python3 this_file.py)

The BIOS packs are manifests (payload/RetroArch/bios/biospack.txt, ...): sha256, size, url - the files are downloaded
at install time and never committed. This test fails when a tracked file looks like a Sony BIOS dump."""

import fnmatch
import os
import shutil
import subprocess
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

SONY_NAMES = ("scph*.bin", "scph*.rom", "psxonpsp*.bin", "romw.bin", "romjp.bin", "ps1_rom.bin")
BIOS_SIZE = 512 * 1024  # every PlayStation BIOS image is 512 KiB
BIOS_EXTENSIONS = (".bin", ".rom", ".bios", ".img")
SONY_MARKER = b"Sony Computer Entertainment Inc."


def tracked_files():
    out = subprocess.run(["git", "-C", REPO, "ls-files", "-z"], check=True, capture_output=True).stdout
    return [p for p in out.decode("utf-8").split("\0") if p]


@unittest.skipUnless(shutil.which("git") and os.path.exists(os.path.join(REPO, ".git")), "not a git checkout")
class NoSonyBiosTest(unittest.TestCase):
    def test_no_tracked_file_is_named_like_a_sony_bios(self):
        bad = [p for p in tracked_files()
               if any(fnmatch.fnmatch(os.path.basename(p).lower(), pat) for pat in SONY_NAMES)]
        self.assertEqual(bad, [], "a Sony BIOS name is tracked")

    def test_no_tracked_bios_sized_file_carries_the_sony_banner(self):
        bad = []
        for p in tracked_files():
            full = os.path.join(REPO, p)
            if not p.lower().endswith(BIOS_EXTENSIONS) or not os.path.isfile(full) or os.path.islink(full):
                continue
            if os.path.getsize(full) != BIOS_SIZE:
                continue
            with open(full, "rb") as f:
                if SONY_MARKER in f.read():
                    bad.append(p)
        self.assertEqual(bad, [], "a Sony BIOS dump is tracked")

    def test_the_guard_recognises_a_sony_bios(self):
        # the rule itself: a 512 KiB .bin with the banner is caught, the same size without it is not
        sony = b"\0" * 100 + SONY_MARKER + b"\0" * (BIOS_SIZE - 100 - len(SONY_MARKER))
        self.assertEqual(len(sony), BIOS_SIZE)
        self.assertIn(SONY_MARKER, sony)
        self.assertNotIn(SONY_MARKER, b"\0" * BIOS_SIZE)
        self.assertTrue(fnmatch.fnmatch("SCPH5501.BIN".lower(), SONY_NAMES[0]))

    def test_the_bios_manifests_hold_hashes_not_files(self):
        manifests = [p for p in tracked_files() if os.path.basename(p).startswith("biospack") and p.endswith(".txt")]
        self.assertTrue(manifests)
        for p in manifests:
            with open(os.path.join(REPO, p), encoding="utf-8") as f:
                for n, line in enumerate(f, 1):
                    if line.startswith("#") or not line.strip():
                        continue
                    sha, size, url = line.split(" ", 3)[:3]
                    self.assertEqual(len(sha), 64, f"{p}:{n}")
                    int(sha, 16)
                    self.assertTrue(size.isdigit(), f"{p}:{n}")
                    self.assertTrue(url.startswith("https://"), f"{p}:{n}")


if __name__ == "__main__":
    unittest.main()
