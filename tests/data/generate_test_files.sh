#!/bin/bash
# Generate proper test disc images for AutoBleem unit tests
#
# This script creates minimal but valid PlayStation-compatible disc images
# in both BIN/CUE and CHD formats for testing.

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== AutoBleem Test File Generator ==="
echo ""

# Check for required tools
check_tool() {
    if ! command -v "$1" &> /dev/null; then
        echo "ERROR: $1 is not installed"
        echo "Install with: sudo apt-get install $2"
        exit 1
    fi
}

echo "Checking for required tools..."
check_tool genisoimage genisoimage
check_tool chdman mame-tools

echo "✓ All tools available"
echo ""

# Clean up old files
echo "Cleaning up old test files..."
rm -f test.iso test.bin test.cue test.chd
rm -rf test_disc

# Create minimal disc structure
echo "Creating minimal disc structure..."
mkdir -p test_disc/SYSTEM

# Create SYSTEM.CNF (PlayStation system config)
cat > test_disc/SYSTEM/SYSTEM.CNF << 'EOF'
BOOT = cdrom:\PSX.EXE;1
TCB = 4
EVENT = 16
STACK = 801FFF00
EOF

# Create a dummy PSX.EXE (not bootable, just for structure)
# This is a minimal 2048-byte PS-X EXE header
cat > test_disc/PSX.EXE << 'EOF'
PS-X EXE
AutoBleem Test Executable - Not Bootable
This is a minimal test disc for AutoBleem unit tests.
EOF

# Pad to 2048 bytes
truncate -s 2048 test_disc/PSX.EXE

# Create README
cat > test_disc/README.TXT << 'EOF'
AutoBleem Test Disc Image

This is a minimal PlayStation disc image created for AutoBleem unit tests.
It is NOT a bootable game - it only contains the structure needed to test
disc image reading functionality.

Created by: generate_test_files.sh
Purpose: Unit testing CHDReader and CDReader classes
EOF

echo "✓ Disc structure created"
echo ""

# Create ISO with proper PlayStation format
echo "Creating ISO9660 image..."
genisoimage \
    -o test.iso \
    -V "TEST_DISC" \
    -sysid "PLAYSTATION" \
    -volset "AUTOBLEEM_TEST" \
    -p "AutoBleem Project" \
    -publisher "AutoBleem" \
    -appid "AUTOBLEEM_TEST" \
    -iso-level 2 \
    -xa \
    -rational-rock \
    -quiet \
    test_disc/ > /dev/null 2>&1

echo "✓ ISO created: $(ls -lh test.iso | awk '{print $5}')"
echo ""

# Create BIN file (RAW MODE1/2352 format)
echo "Converting to RAW BIN format (MODE1/2352)..."

# The ISO is MODE1/2048, we need to convert to MODE1/2352 for proper testing
# We'll create a CUE file that references it correctly

cat > test.cue << EOF
FILE "test.bin" BINARY
  TRACK 01 MODE1/2352
    INDEX 01 00:00:00
EOF

# Convert ISO to RAW BIN using MODE1/2352
# This adds the 304 bytes of sync/header/ecc per sector
python3 << 'PYTHON_SCRIPT'
import struct

# Read the MODE1/2048 ISO
with open('test.iso', 'rb') as f:
    iso_data = f.read()

# Calculate number of sectors
sector_count = (len(iso_data) + 2047) // 2048

# Create MODE1/2352 BIN
with open('test.bin', 'wb') as f:
    for sector_num in range(sector_count):
        # Calculate position in ISO
        offset = sector_num * 2048

        # Get sector data (pad if needed)
        if offset < len(iso_data):
            remaining = min(2048, len(iso_data) - offset)
            sector_data = iso_data[offset:offset + remaining]
            sector_data += b'\x00' * (2048 - remaining)
        else:
            sector_data = b'\x00' * 2048

        # Build MODE1/2352 sector:
        # 12 bytes: Sync pattern
        sync = b'\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00'

        # 4 bytes: Header (MSF address + mode)
        # Convert LBA to MSF (Minutes, Seconds, Frames)
        lba = sector_num + 150  # Offset for CD start
        m = lba // (60 * 75)
        s = (lba // 75) % 60
        frm = lba % 75
        header = bytes([m, s, frm, 0x01])  # Mode 1

        # 2048 bytes: User data
        # (already have this)

        # 288 bytes: Error correction (just zeros for test)
        ecc = b'\x00' * 288

        # Write complete 2352-byte sector
        f.write(sync + header + sector_data + ecc)

print(f"Created test.bin with {sector_count} sectors ({sector_count * 2352} bytes)")
PYTHON_SCRIPT

echo "✓ BIN created: $(ls -lh test.bin | awk '{print $5}')"
echo ""

# Create CHD from BIN/CUE
echo "Creating CHD compressed image..."
chdman createcd \
    -i test.cue \
    -o test.chd \
    -c zstd \
    > /dev/null 2>&1

echo "✓ CHD created: $(ls -lh test.chd | awk '{print $5}')"
echo ""

# Verify files
echo "Verifying test files..."

# Check ISO has CD001
if hexdump -C test.iso | grep -q "CD001"; then
    echo "✓ test.iso has valid ISO9660 signature"
else
    echo "✗ WARNING: test.iso missing CD001 signature"
fi

# Check BIN has CD001
if hexdump -C test.bin | grep -q "CD001"; then
    echo "✓ test.bin has valid ISO9660 signature"
else
    echo "✗ WARNING: test.bin missing CD001 signature"
fi

# Check CHD
if chdman info -i test.chd > /dev/null 2>&1; then
    echo "✓ test.chd is valid CHD file"
    echo ""
    echo "CHD Info:"
    chdman info -i test.chd | grep -E "^(Logical|Hunk|Compression|SHA1)" | sed 's/^/  /'
else
    echo "✗ ERROR: test.chd is invalid"
    exit 1
fi

echo ""

# Clean up intermediate files
echo "Cleaning up..."
rm -rf test_disc
# Keep test.bin and test.cue for reference/debugging

echo ""
echo "=== Generation Complete ==="
echo ""
echo "Created test files:"
echo "  test.iso  - MODE1/2048 ISO9660 image"
echo "  test.bin  - MODE1/2352 RAW CD image"
echo "  test.cue  - CUE sheet for test.bin"
echo "  test.chd  - Compressed CHD image"
echo ""
echo "You can now run tests:"
echo "  cd ../../build_sys"
echo "  ./tests/chd_reader_test"
echo ""
