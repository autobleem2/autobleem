#!/usr/bin/env python3
"""tools/make_pe_gamecontrollerdb.py: the trimmed pad table for PE apps (run by ctest, or: python3 this_file.py)"""

import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))

import make_pe_gamecontrollerdb as pe  # noqa: E402

SHIPPED = os.path.join(REPO, "src", "resources", "gamecontrollerdb.txt")


class TrimTest(unittest.TestCase):
    def test_shipped_table_fits_one_environment_string_with_room(self):
        with open(SHIPPED, encoding="utf-8-sig") as f:
            text = f.read()
        self.assertGreater(len(text.encode("utf-8")), 131072, "the full table is the problem this tool solves")
        out = pe.trim(text)
        self.assertLess(len(out.encode("utf-8")), 100 * 1024)
        self.assertGreater(out.count("\n"), 200, "a useful number of pads is kept")

    def test_linux_only_and_console_pad_first_in_its_original_form(self):
        with open(SHIPPED, encoding="utf-8-sig") as f:
            out = pe.trim(f.read())
        lines = [l for l in out.splitlines() if l and not l.startswith("#")]
        self.assertEqual(lines[0], pe.PSC_LINE)
        self.assertIn("leftx:a0,lefty:a1", lines[0])
        for line in lines:
            self.assertIn("platform:Linux", line)
        # one line for the console's GUID
        self.assertEqual(sum(1 for l in lines if l.startswith(pe.PSC_GUID)), 1)
        # no GUID twice
        guids = [l.split(",", 1)[0] for l in lines]
        self.assertEqual(len(guids), len(set(guids)))

    def test_makers_in_order_then_the_rest_and_the_limit_is_kept(self):
        def line(guid, name):
            return "%s,%s,a:b0,platform:Linux" % (guid, name)

        table = "\n".join(
            [
                "# Linux",
                line("03000000aaaa00000000000011010000", "Unknown maker"),
                line("030000005e0400000000000011010000", "Xbox"),
                line("030000004c0500000000000011010000", "DualShock"),
                "03000000ffff00000000000000000000,Windows pad,a:b0,platform:Windows",
            ]
        )
        out = pe.trim(table)
        names = [l.split(",")[1] for l in out.splitlines() if l and not l.startswith("#")]
        self.assertEqual(names, ["Sony Interactive Entertainment Controller", "Xbox", "DualShock", "Unknown maker"])
        small = pe.trim(table, limit=len(pe.trim("")) + 80)
        self.assertLess(len(small), len(out))
        self.assertNotIn("Unknown maker", small)

    def test_vendor_of(self):
        self.assertEqual(pe.vendor_of("030000004c050000da0c000011010000,x"), "054c")
        self.assertEqual(pe.vendor_of("not a guid,x"), "")


if __name__ == "__main__":
    unittest.main()
