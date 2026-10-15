#!/usr/bin/env python3
"""tools/make_win_package.sh: pcsx-abnxt gets a fonts/ folder beside its exe (run by ctest, or: python3 this_file.py)"""

import os
import re
import shutil
import subprocess
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
SCRIPT = os.path.join(REPO, "tools", "make_win_package.sh")


def function_text():
    with open(SCRIPT, encoding="utf-8") as f:
        text = f.read()
    match = re.search(r"^emulator_fonts\(\) \{\n.*?^\}\n", text, re.S | re.M)
    assert match, "emulator_fonts() is gone from make_win_package.sh"
    return match.group(0)


class WinEmulatorFontsTest(unittest.TestCase):
    def test_the_product_step_calls_it_for_the_nxt_emulator(self):
        with open(SCRIPT, encoding="utf-8") as f:
            self.assertIn('emulator_fonts "pcsx-abnxt" "$APP/emunxt"', f.read())

    @unittest.skipUnless(shutil.which("bash"), "no bash")
    def run_fn(self, name, dist_fonts):
        with tempfile.TemporaryDirectory() as tmp:
            dest = os.path.join(tmp, "emunxt")
            dist = os.path.join(tmp, "dist")
            os.makedirs(dest)
            open(os.path.join(dest, "pcsx-ab.exe"), "w").close()
            if dist_fonts:
                os.makedirs(os.path.join(dist, "fonts"))
                open(os.path.join(dist, "fonts", "Mine.otf"), "w").close()
            script = function_text() + 'REPO="$1"; emulator_fonts "$2" "$3" "$4"\n'
            subprocess.run(["bash", "-c", script, "x", REPO, name, dest, dist if dist_fonts else ""], check=True)
            fonts = os.path.join(dest, "fonts")
            return sorted(os.listdir(fonts)) if os.path.isdir(fonts) else None

    def test_bundled_chinese_font_is_the_fallback(self):
        self.assertEqual(self.run_fn("pcsx-abnxt", False), ["NotoSansSC-Regular.otf", "OFL.txt"])

    def test_the_builds_own_fonts_win(self):
        self.assertEqual(self.run_fn("pcsx-abnxt", True), ["Mine.otf"])

    def test_the_other_emulator_is_left_alone(self):
        self.assertIsNone(self.run_fn("pcsx-ab", False))


if __name__ == "__main__":
    unittest.main()
