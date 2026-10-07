#!/usr/bin/env python3
"""The PE runner in the Raspberry Pi 32-bit package (APPS-13; run by ctest, or: python3 this_file.py).

The Pi's rc/ comes from payload_linux/Autobleem/rc (publish-launcher.yml); the PE runner files live only in payload/
(the console's), so the workflow's rpi branch copies them - one copy, never a duplicate - together with the pad table and
the abdialog program, and no other Linux target gets them (the PE ports are built for the Pi alone)."""

import os
import re
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
WORKFLOW = os.path.join(REPO, ".github", "workflows", "publish-launcher.yml")
RUNNER = ("pe_run.sh", "pe_env.sh", "pe_compat.ini")
DIALOGS = ("sdl_text_display", "sdl_input_text_display", "sdl_display")


def read(*parts):
    with open(os.path.join(REPO, *parts), encoding="utf-8") as f:
        return f.read()


def rpi_blocks(text):
    """The workflow's shell blocks that are guarded by `matrix.target }}" = rpi ]`."""
    blocks = []
    for m in re.finditer(r'if \[ "\$\{\{ matrix\.target \}\}" = rpi \]; then\n(.*?)\n\s*fi\n', text, re.S):
        blocks.append(m.group(1))
    return blocks


class PeRpiPackageTest(unittest.TestCase):
    def test_the_runner_files_exist_in_the_console_payload_only(self):
        for name in RUNNER:
            self.assertTrue(os.path.isfile(os.path.join(REPO, "payload", "Autobleem", "rc", name)), name)
            self.assertFalse(os.path.exists(os.path.join(REPO, "payload_linux", "Autobleem", "rc", name)),
                             name + " must not be duplicated in payload_linux")

    def test_the_rpi_package_gets_the_runner_the_pad_table_and_abdialog(self):
        blocks = "\n".join(rpi_blocks(read(".github", "workflows", "publish-launcher.yml")))
        for name in RUNNER:
            self.assertIn("payload/Autobleem/rc/" + name, blocks)
        self.assertIn("make_pe_gamecontrollerdb.py", blocks)
        self.assertIn("stage/Autobleem/rc/pe_gamecontrollerdb.txt", blocks)
        self.assertIn('cp "$D/abdialog"', blocks)

    def test_the_rpi_package_gets_the_three_dialog_scripts(self):
        # a mod's launch.sh calls ${PROJECT_ERIS_PATH}/bin/sdl_input_text_display: pe_env.sh copies it from rc/pe/ -
        # without the script the call is "not found" (127) and the mod's question (TyrQuake's pad choice, DOSBox's
        # game choice) is never asked (the Pi's first round, 2026-10-07)
        blocks = "\n".join(rpi_blocks(read(".github", "workflows", "publish-launcher.yml")))
        for name in DIALOGS:
            self.assertTrue(os.path.isfile(os.path.join(REPO, "payload", "Autobleem", "rc", "pe", name)), name)
            self.assertIn("payload/Autobleem/rc/pe/" + name, blocks)
        self.assertIn("stage/Autobleem/rc/pe/", blocks)
        self.assertNotIn("rc/pe/lib", blocks)  # the console's gl4es stays out of the Pi's package

    def test_no_other_linux_target_gets_them(self):
        text = read(".github", "workflows", "publish-launcher.yml")
        # outside the rpi-guarded blocks the runner is not named
        for block in rpi_blocks(text):
            text = text.replace(block, "")
        self.assertNotIn("pe_run.sh", text)
        self.assertNotIn("pe_gamecontrollerdb", text)

    def test_the_trimmed_pad_table_tool_is_there(self):
        self.assertTrue(os.path.isfile(os.path.join(REPO, "tools", "make_pe_gamecontrollerdb.py")))


if __name__ == "__main__":
    unittest.main()
