#!/usr/bin/env python3
"""The Sony-hash guard: no Sony PlayStation BIOS image is tracked in this repository (run by ctest, or: python3 this_file.py)

The BIOS packs are manifests (payload/RetroArch/bios/biospack.txt, ...): sha256, size, url - the files are downloaded
at install time and never committed. This test fails when a tracked file looks like a Sony BIOS dump."""

import fnmatch
import hashlib
import os
import shutil
import subprocess
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

SONY_NAMES = ("scph*.bin", "scph*.rom", "psxonpsp*.bin", "romw.bin", "romjp.bin", "ps1_rom.bin")
BIOS_SIZE = 512 * 1024  # every PlayStation BIOS image is 512 KiB
BIOS_EXTENSIONS = (".bin", ".rom", ".bios", ".img")
SONY_MARKER = b"Sony Computer Entertainment Inc."


HASHES = os.path.join(HERE, "sony_bios_hashes.txt")
SCANNED = ("payload", os.path.join("src", "resources"))
CHUNK = 1 << 20


def load_known_hashes():
    known = {}
    with open(HASHES, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            algo, digest, name = line.rstrip("\n").split(" ", 2)
            known[(algo, digest.lower())] = name
    return known


def walk(folder):
    for root, _dirs, files in os.walk(folder):
        for name in sorted(files):
            path = os.path.join(root, name)
            if os.path.isfile(path):
                yield path


def file_hashes(path):
    sha, md5 = hashlib.sha256(), hashlib.md5()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(CHUNK)
            if not chunk:
                break
            sha.update(chunk)
            md5.update(chunk)
    return sha.hexdigest(), md5.hexdigest()


def match_known(path, known):
    sha, md5 = file_hashes(path)
    return known.get(("sha256", sha)) or known.get(("md5", md5))


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

    def test_no_file_in_payload_or_resources_hashes_like_a_known_bios(self):
        known = load_known_hashes()
        self.assertGreater(len(known), 30)
        for folder in SCANNED:
            for path in walk(os.path.join(REPO, folder)):
                self.assertEqual(match_known(path, known), None, f"{path}: hashes like a known Sony BIOS image")

    def test_an_injected_hash_makes_the_scan_fail_with_the_path(self):
        with tempfile.TemporaryDirectory() as tmp:
            planted = os.path.join(tmp, "sub", "innocent.dat")
            os.makedirs(os.path.dirname(planted))
            with open(planted, "wb") as f:
                f.write(b"not a bios, but its hash is on the list\n" * 100000)  # > one chunk
            with open(os.path.join(tmp, "other.dat"), "wb") as f:
                f.write(b"other")
            sha, md5 = file_hashes(planted)
            for algo, digest in (("sha256", sha), ("md5", md5)):
                known = {(algo, digest): "injected"}
                found = [p for p in walk(tmp) if match_known(p, known)]
                self.assertEqual(found, [planted], algo)
            self.assertEqual([p for p in walk(tmp) if match_known(p, load_known_hashes())], [])

    def test_the_hash_list_is_hashes_only(self):
        with open(HASHES, encoding="utf-8") as f:
            for n, line in enumerate(f, 1):
                if line.startswith("#") or not line.strip():
                    continue
                algo, digest, name = line.rstrip("\n").split(" ", 2)
                self.assertEqual(len(digest), {"md5": 32, "sha256": 64}[algo], f"line {n}")
                int(digest, 16)
        self.assertLess(os.path.getsize(HASHES), 20000)

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
