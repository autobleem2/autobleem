#!/usr/bin/env python3
"""tools/fake_ra_runner.sh: the scenarios write the contract's lines (src/code/evoui/ra_job.h) with the mockups' numbers
(run by ctest, or: python3 this_file.py)"""

import os
import re
import subprocess
import sys
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
RUNNER = os.path.abspath(os.path.join(HERE, "..", "..", "tools", "fake_ra_runner.sh"))

PHASE = re.compile(r"^phase (\d+)/(\d+)\|([a-z-]+)\|(\d+)\|(\d+)\|(bytes|files|items)\|([^|]*)\|(\d+)\|(\d+)$")


def run(op, env_extra, args=()):
    """Runs the runner and samples its progress file; returns (exit code, lines seen in order, no repeats)"""
    with tempfile.TemporaryDirectory() as tmp:
        progress = os.path.join(tmp, "progress")
        env = dict(os.environ, AB_FAKE_RA_STEPS="2", AB_FAKE_RA_HOLD="1")
        for name in [k for k in env if k.startswith("AB_FAKE_RA_") and k not in ("AB_FAKE_RA_STEPS", "AB_FAKE_RA_HOLD")]:
            del env[name]
        env.update(env_extra)
        proc = subprocess.Popen(["sh", RUNNER, op, *args, "--progress", progress], env=env)
        seen = []
        while True:
            done = proc.poll() is not None
            try:
                with open(progress, encoding="utf-8") as f:
                    line = f.read().strip()
            except OSError:
                line = ""
            if line and (not seen or seen[-1] != line):
                seen.append(line)
            if done:
                break
            time.sleep(0.02)
        return proc.returncode, seen


def phases(seen):
    out = []
    for line in seen:
        m = PHASE.match(line)
        if m:
            out.append((int(m[1]), int(m[2]), m[3], int(m[4]), int(m[5]), m[6], m[7], int(m[8]), int(m[9])))
    return out


class FakeRaRunnerTest(unittest.TestCase):
    def assertGrammar(self, seen):
        for line in seen[:-1]:
            self.assertRegex(line, PHASE)
        for p in phases(seen):
            self.assertTrue(1 <= p[0] <= p[1] <= 99)
            self.assertTrue(0 < p[4] and p[3] <= p[4])  # a total above 0, done within it
            self.assertTrue(p[7] <= p[8])

    def test_default_walk_is_unchanged(self):
        code, seen = run("install", {})
        self.assertEqual(code, 0)
        self.assertEqual(seen[-1], "end|ok")
        self.assertTrue(all(p[7:] == (1, 1) for p in phases(seen)))
        self.assertEqual({p[1] for p in phases(seen)}, {9})

    def test_full_has_nine_phases_with_varying_packages(self):
        code, seen = run("install", {"AB_FAKE_RA_SCENARIO": "full"})
        self.assertEqual(code, 0)
        self.assertEqual(seen[-1], "end|ok")
        self.assertGrammar(seen)
        ph = phases(seen)
        self.assertEqual({p[1] for p in ph}, {9})
        self.assertEqual({p[0] for p in ph}, set(range(1, 10)))
        self.assertTrue(any(p[7:] == (3, 9) and p[2] == "download-cores" and p[4] == 289000000 for p in ph))
        self.assertTrue(any(p[2] == "unpack" and p[5] == "files" and p[6] == "cheats.zip" and p[4] == 41000 for p in ph))
        self.assertTrue(any(p[2] == "unpack" and p[5] == "bytes" and p[6].startswith("cores") for p in ph))

    def test_full_with_bios_has_ten_phases(self):
        code, seen = run("install", {"AB_FAKE_RA_SCENARIO": "full", "AB_FAKE_RA_STEPS": "1"}, ["--bios"])
        self.assertEqual(code, 0)
        self.assertEqual({p[1] for p in phases(seen)}, {10})
        self.assertIn("download-bios", {p[2] for p in phases(seen)})

    def test_cores_holds_at_128_of_289(self):
        code, seen = run("install", {"AB_FAKE_RA_SCENARIO": "cores", "AB_FAKE_RA_MBPS10": "240"})
        self.assertEqual(code, 0)
        self.assertGrammar(seen)
        held = [p for p in phases(seen) if 128000000 <= p[3] < 128100000]
        self.assertTrue(held)
        self.assertTrue(all(p[:3] == (3, 9, "download-cores") and p[4] == 289000000 and p[7:] == (3, 9) for p in held))

    def test_cheats_and_targz_units(self):
        code, seen = run("install", {"AB_FAKE_RA_SCENARIO": "cheats"})
        self.assertEqual(code, 0)
        self.assertIn("phase 7/9|unpack|12400|41000|files|cheats.zip|5|9", seen)
        code, seen = run("install", {"AB_FAKE_RA_SCENARIO": "targz"})
        self.assertEqual(code, 0)
        self.assertIn("phase 7/9|unpack|120000000|290000000|bytes|cores-psc-20261003.tar.gz|3|9", seen)

    def test_remove7_and_remove8(self):
        code, seen = run("remove", {"AB_FAKE_RA_SCENARIO": "remove7"})
        self.assertEqual(code, 0)
        self.assertGrammar(seen)
        self.assertIn("phase 1/1|remove-files|3120|7400|files|Emulator_cores|2|7", seen)
        self.assertEqual({p[8] for p in phases(seen)}, {7})
        code, seen = run("remove", {"AB_FAKE_RA_SCENARIO": "remove7"}, ["--bios"])
        self.assertEqual({p[8] for p in phases(seen)}, {8})
        self.assertEqual(phases(seen)[-1][6], "BIOS_files")

    def test_remove_can_fail_and_die(self):
        code, seen = run("remove", {"AB_FAKE_RA_SCENARIO": "remove7", "AB_FAKE_RA_FAIL": "other"})
        self.assertEqual(code, 1)
        self.assertEqual(seen[-1], "end|error|other")
        code, seen = run("remove", {"AB_FAKE_RA_FAIL": "other", "AB_FAKE_RA_STEPS": "5"})  # the plain walk too
        self.assertEqual(seen[-1], "end|error|other")
        code, seen = run("remove", {"AB_FAKE_RA_SCENARIO": "remove7", "AB_FAKE_RA_DIE": "1"})
        self.assertNotEqual(code, 0)
        self.assertFalse(seen[-1].startswith("end"))

    def test_install_fails_with_the_code(self):
        for scenario in ("full", "cores"):
            code, seen = run("install", {"AB_FAKE_RA_SCENARIO": scenario, "AB_FAKE_RA_FAIL": "no-space"})
            self.assertEqual(code, 1)
            self.assertEqual(seen[-1], "end|error|no-space")

    def test_version_stamp_beside_the_binary(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = os.path.join(tmp, "RetroArch", "bin", "retroarch")
            env = {"AB_FAKE_RA_BINARY": binary, "AB_FAKE_RA_VERSION": "v1.23.0"}
            run("install", env)
            with open(os.path.join(tmp, "RetroArch", "bin", "VERSION"), encoding="utf-8") as f:
                self.assertEqual(f.read().strip(), "v1.23.0")
            run("remove", env)
            self.assertFalse(os.path.exists(binary))
            self.assertFalse(os.path.exists(os.path.join(tmp, "RetroArch", "bin", "VERSION")))

    def test_unknown_scenario_is_refused(self):
        code, _ = run("install", {"AB_FAKE_RA_SCENARIO": "nonsense"})
        self.assertEqual(code, 2)


if __name__ == "__main__":
    unittest.main()
