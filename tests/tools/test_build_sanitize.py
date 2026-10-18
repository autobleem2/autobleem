"""ci/build.sh: AB_SANITIZE reaches the native and the pcusb configure only, and an unset AB_SANITIZE passes the
CMake default (empty), so a normal build is the same as before."""
import re
import subprocess
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / "ci" / "build.sh"


class BuildSanitizeTest(unittest.TestCase):
    def setUp(self):
        self.text = SCRIPT.read_text()

    def test_syntax(self):
        subprocess.run(["bash", "-n", str(SCRIPT)], check=True)

    def test_native_and_pcusb_take_the_switch(self):
        m = re.search(r'case "\$dir" in ([a-z_ |]+)\) san=', self.text)
        self.assertIsNotNone(m)
        dirs = [d.strip() for d in m.group(1).split("|")]
        # (the symbols build of the stick, AB_PCUSB_DEBUG=1, has its own directory and takes the switch too)
        self.assertEqual(sorted(dirs), ["build_pcusb", "build_pcusb_dbg", "build_pcusb_san", "build_sys"])

    def test_unset_means_empty_default(self):
        self.assertIn('-DAB_SANITIZE=${AB_SANITIZE:-}', self.text)

    def test_other_targets_do_not_take_it(self):
        for d in ("build_psc", "build_rpi", "build_rpi64", "build_win"):
            self.assertNotIn(d + " ) san", self.text)


if __name__ == "__main__":
    unittest.main()
