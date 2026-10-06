#!/usr/bin/env python3
"""tools/unshipped_themes.*: the theme folders no package ships (run by ctest, or: python3 this_file.py)"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))

import unshipped_themes  # noqa: E402

THEMES = ("ab2", "ab2.0.0", "aergb", "default", "evolution")


class UnshippedThemesTest(unittest.TestCase):
    def test_ab2_is_listed_and_the_default_theme_is_not(self):
        names = unshipped_themes.names()
        self.assertIn("ab2", names)
        self.assertNotIn("ab2.0.0", names)  # the default theme shares the prefix, not the name
        self.assertNotIn("default", names)

    def test_the_packaging_scripts_apply_the_list(self):
        for script in ("make_psc_package.sh", "make_win_package.sh"):
            with open(os.path.join(REPO, "tools", script), encoding="utf-8") as f:
                text = f.read()
            self.assertIn('drop_unshipped_themes "', text, script)
        for script in ("make_usb.py", "install_autobleem.py"):
            with open(os.path.join(REPO, "tools", script), encoding="utf-8") as f:
                self.assertIn("unshipped_themes.names()", f.read(), script)

    @unittest.skipUnless(shutil.which("bash"), "no bash")
    def test_the_shell_helper_drops_only_the_listed_folders(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name in THEMES:
                os.makedirs(os.path.join(tmp, "Themes", name))
                with open(os.path.join(tmp, "Themes", name, "theme.json"), "w") as f:
                    f.write("{}")
            helper = os.path.join(REPO, "tools", "unshipped_themes.sh").replace("\\", "/")
            themes = os.path.join(tmp, "Themes").replace("\\", "/")
            subprocess.run(["bash", "-c", '. "$1"; drop_unshipped_themes "$2"', "bash", helper, themes], check=True,
                           stdout=subprocess.DEVNULL)
            left = sorted(os.listdir(os.path.join(tmp, "Themes")))
            self.assertEqual(left, sorted(n for n in THEMES if n != "ab2"))


if __name__ == "__main__":
    unittest.main()
