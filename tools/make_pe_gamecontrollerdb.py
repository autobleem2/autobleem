#!/usr/bin/env python3
"""
The pad table PE apps get: a trimmed copy of src/resources/gamecontrollerdb.txt, under 100 KB.

    python tools/make_pe_gamecontrollerdb.py src/resources/gamecontrollerdb.txt OUT [--limit BYTES]

A PE mod passes its pad table to the game as SDL_GAMECONTROLLERCONFIG="$(cat .../gamecontrollerdb.txt)" - one
environment string, which Linux caps at 128 KB (MAX_ARG_STRLEN): our full table is 526 KB and would make execve fail
with "Argument list too long". So the file rc/pe_run.sh puts at ${PROJECT_ERIS_PATH}/etc/boot_menu/ is this one:
  - the Linux lines only (the console runs Linux);
  - first the console's own pad, written the way the mods were made for it (the d-pad as the left stick,
    leftx:a0,lefty:a1) - it replaces our line for that GUID;
  - then the pads people plug in, by maker (Microsoft, Sony, 8BitDo, Nintendo, Valve, Logitech, ...), then the rest
    in the file's order, until the limit (95 000 bytes by default) is reached.
Run at build time by tools/make_psc_package.sh, which lays the result at Autobleem/rc/pe_gamecontrollerdb.txt.
"""

import argparse
import sys

# the console's pad as the mods expect it (the original table's first line)
PSC_GUID = "030000004c050000da0c000011010000"
PSC_LINE = (
    PSC_GUID + ",Sony Interactive Entertainment Controller,x:b3,a:b2,b:b1,y:b0,back:b8,start:b9,"
    "leftshoulder:b6,lefttrigger:b4,rightshoulder:b7,righttrigger:b5,leftx:a0,lefty:a1,platform:Linux"
)

# USB vendor ids (as the GUID holds them: bytes 4-5 of the vendor field, little endian), most common first
VENDOR_ORDER = [
    "045e",  # Microsoft
    "054c",  # Sony
    "2dc8",  # 8BitDo
    "057e",  # Nintendo
    "28de",  # Valve
    "046d",  # Logitech
    "0f0d",  # Hori
    "0e6f",  # PDP
    "24c6",  # PowerA
    "20d6",  # PowerA
    "1532",  # Razer
    "0079",  # generic DragonRise
    "0810",  # generic
    "2563",  # generic
    "1a34",  # ACRUX
    "0738",  # Mad Catz
    "1bad",  # Harmonix / Mad Catz
    "12bd",  # generic
]

DEFAULT_LIMIT = 95000


def vendor_of(line):
    guid = line.split(",", 1)[0].lower()
    if len(guid) != 32:
        return ""
    # a USB GUID: bus(2) 00 crc(2) vendor(4, little endian) 0000 product ...
    return guid[10:12] + guid[8:10]


def trim(text, limit=DEFAULT_LIMIT):
    """the trimmed table (a string) made from the full one"""
    linux = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "platform:Linux" not in line:
            continue
        if line.split(",", 1)[0].lower() == PSC_GUID:
            continue  # the console's pad is written our way below
        linux.append(line)

    rank = {v: i for i, v in enumerate(VENDOR_ORDER)}
    ordered = sorted(enumerate(linux), key=lambda p: (rank.get(vendor_of(p[1]), len(VENDOR_ORDER)), p[0]))

    header = (
        "# Game Controller DB for the PE apps - a trimmed copy of AutoBleem's table (tools/make_pe_gamecontrollerdb.py)\n"
        "# Linux only; the console's pad first, as the mods expect it\n"
    )
    out = [PSC_LINE]
    size = len(header) + len(PSC_LINE) + 1
    seen = {PSC_GUID}
    for _, line in ordered:
        guid = line.split(",", 1)[0].lower()
        if guid in seen:
            continue
        if size + len(line) + 1 > limit:
            continue  # a shorter line further on may still fit
        seen.add(guid)
        out.append(line)
        size += len(line) + 1
    return header + "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("source")
    ap.add_argument("out")
    ap.add_argument("--limit", type=int, default=DEFAULT_LIMIT)
    args = ap.parse_args()
    with open(args.source, encoding="utf-8-sig") as f:
        text = f.read()
    result = trim(text, args.limit)
    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        f.write(result)
    print("%s: %d bytes, %d pads" % (args.out, len(result.encode("utf-8")), result.count("\n") - 2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
