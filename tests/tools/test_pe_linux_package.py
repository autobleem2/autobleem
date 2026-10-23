#!/usr/bin/env python3
"""The PE runner in the Raspberry Pi packages, 32 and 64-bit (run by ctest, or: python3 this_file.py).

The Pi's rc/ comes from payload_linux/Autobleem/rc (publish-launcher.yml); the PE runner files live only in payload/
(the console's), so the workflow's rpi, rpi64 and pcusb branches copy them - one copy, never a duplicate - together with
the pad table and the abdialog program. No other target gets them: psc has them in its own payload, and windows
stages no rc/ at all."""

import os
import re
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
WORKFLOW = os.path.join(REPO, ".github", "workflows", "publish-launcher.yml")
RUNNER = ("pe_run.sh", "pe_env.sh", "pe_compat.ini")
DIALOGS = ("sdl_text_display", "sdl_input_text_display", "sdl_display")
# the targets that ship the PE runner, exactly; pcusb, psc and win are named here so a change has to touch this list
PE_TARGETS = ("rpi", "rpi64", "pcusb")
NO_PE_TARGETS = ("psc", "win")

# `if [ "${{ matrix.target }}" = rpi ] || [ "${{ matrix.target }}" = rpi64 ]; then`, up to the `fi` of the same
# indentation (a block of another target may hold an inner if, so the first `fi` is not the end)
GUARD = re.compile(r'^( *)if (\[ "\$\{\{ matrix\.target \}\}" = \w+ \](?: \|\| \[ "\$\{\{ matrix\.target \}\}" = \w+ \])*); then\n'
                   r'(.*?)\n\1fi\n', re.S | re.M)


def read(*parts):
    with open(os.path.join(REPO, *parts), encoding="utf-8") as f:
        return f.read()


def pe_blocks(text):
    """(targets, body) of the workflow's shell blocks guarded by a `matrix.target = X` test list that name the PE runner
    or abdialog; the targets are the names the guard accepts."""
    blocks = []
    for m in GUARD.finditer(text):
        inner = pe_blocks(m.group(3))  # finditer does not overlap: look inside every block for the nested ones
        targets = tuple(re.findall(r'= (\w+) \]', m.group(2)))
        if inner:
            blocks.extend(inner)  # an outer block (psc's if/else) that holds an inner PE block is not itself one
        elif "pe_run.sh" in m.group(3) or 'cp "$D/abdialog"' in m.group(3):
            blocks.append((targets, m.group(3)))
    return blocks


class PeLinuxPackageTest(unittest.TestCase):
    def test_the_runner_files_exist_in_the_console_payload_only(self):
        for name in RUNNER:
            self.assertTrue(os.path.isfile(os.path.join(REPO, "payload", "Autobleem", "rc", name)), name)
            self.assertFalse(os.path.exists(os.path.join(REPO, "payload_linux", "Autobleem", "rc", name)),
                             name + " must not be duplicated in payload_linux")

    def test_the_runner_blocks_are_guarded_for_rpi_rpi64_and_pcusb(self):
        blocks = pe_blocks(read(".github", "workflows", "publish-launcher.yml"))
        self.assertEqual(2, len(blocks), "one block for abdialog, one for the runner files")
        for targets, _ in blocks:
            self.assertEqual(sorted(PE_TARGETS), sorted(targets))

    def test_the_pi_packages_get_the_runner_the_pad_table_and_abdialog(self):
        blocks = "\n".join(b for _, b in pe_blocks(read(".github", "workflows", "publish-launcher.yml")))
        for name in RUNNER:
            self.assertIn("payload/Autobleem/rc/" + name, blocks)
        self.assertIn("make_pe_gamecontrollerdb.py", blocks)
        self.assertIn("stage/Autobleem/rc/pe_gamecontrollerdb.txt", blocks)
        self.assertIn('cp "$D/abdialog"', blocks)

    def test_the_pi_packages_get_the_three_dialog_scripts(self):
        # a mod's launch.sh calls ${PROJECT_ERIS_PATH}/bin/sdl_input_text_display: pe_env.sh copies it from rc/pe/ -
        # without the script the call is "not found" (127) and the mod's question (TyrQuake's pad choice, DOSBox's
        # game choice) is never asked (the Pi's first round, 2026-10-07)
        blocks = "\n".join(b for _, b in pe_blocks(read(".github", "workflows", "publish-launcher.yml")))
        for name in DIALOGS:
            self.assertTrue(os.path.isfile(os.path.join(REPO, "payload", "Autobleem", "rc", "pe", name)), name)
            self.assertIn("payload/Autobleem/rc/pe/" + name, blocks)
        self.assertIn("stage/Autobleem/rc/pe/", blocks)
        self.assertNotIn("rc/pe/lib", blocks)  # the console's gl4es stays out of the Pi's packages

    def test_no_other_linux_target_gets_them(self):
        # CHANGED ON PURPOSE (the PC stick now runs PE ports): the list that may carry the runner is exactly
        # rpi, rpi64 and pcusb. psc (has them in payload/), windows and payload_linux/ stay without.
        text = read(".github", "workflows", "publish-launcher.yml")
        for _, block in pe_blocks(text):
            text = text.replace(block, "")
        self.assertNotIn("pe_run.sh", text)
        self.assertNotIn("pe_gamecontrollerdb", text)
        self.assertNotIn('cp "$D/abdialog" "$BIN/autobleem/"\n                upx', text)
        for target in NO_PE_TARGETS:
            for targets, _ in pe_blocks(read(".github", "workflows", "publish-launcher.yml")):
                self.assertNotIn(target, targets)
        for name in RUNNER + ("pe_gamecontrollerdb.txt",):
            self.assertFalse(os.path.exists(os.path.join(REPO, "payload_linux", "Autobleem", "rc", name)), name)

    def test_the_trimmed_pad_table_tool_is_there(self):
        self.assertTrue(os.path.isfile(os.path.join(REPO, "tools", "make_pe_gamecontrollerdb.py")))


if __name__ == "__main__":
    unittest.main()
