#!/bin/bash
set -e

DEVICE=$1

if [ -z "$DEVICE" ]; then
    echo "Usage: ./flash.sh <device>"
    echo "Example: ./flash.sh /dev/mmcblk0"
    exit 1
fi

if [ ! -b "$DEVICE" ]; then
    echo "Error: $DEVICE is not a block device"
    exit 1
fi

REPO_DIR=$(dirname "$(realpath "$0")")/..
FLASH_BIN="$REPO_DIR/blob/flash.bin"
ZKOS_BIN="$REPO_DIR/boot/minimal/asm-lang/zkos-minimal.bin"

if [ ! -f "$FLASH_BIN" ]; then
    echo "Error: flash.bin not found at $FLASH_BIN"
    exit 1
fi

if [ ! -f "$ZKOS_BIN" ]; then
    echo "Building zkos-minimal.bin..."
    make -C "$REPO_DIR/boot/minimal/asm-lang"
fi

echo ""
echo "  Device:    $DEVICE"
echo "  flash.bin: $FLASH_BIN ($(stat -c%s "$FLASH_BIN") bytes)"
echo "  zkos.bin:  $ZKOS_BIN ($(stat -c%s "$ZKOS_BIN") bytes)"
echo ""
echo "WARNING: ALL DATA ON $DEVICE WILL BE DESTROYED!"
read -p "Type 'yes' to continue: " CONFIRM
if [ "$CONFIRM" != "yes" ]; then
    echo "Aborted."
    exit 1
fi

# Unmount
umount ${DEVICE}* 2>/dev/null || true

# Clear first 8MB
echo "[1/5] Clearing first 8MB..."
dd if=/dev/zero of=$DEVICE bs=1M count=8 status=none

# Write flash.bin at 32KB offset (required by i.MX93 BootROM)
echo "[2/5] Writing flash.bin at 32KB offset..."
dd if=$FLASH_BIN of=$DEVICE bs=1K seek=32 status=none

# Create FAT32 partition starting at 4MB
echo "[3/5] Creating FAT32 partition..."
sfdisk -q $DEVICE <<EOF
4M,,c
EOF

# Format
PART="${DEVICE}p1"
[ -b "$PART" ] || PART="${DEVICE}1"
echo "[4/5] Formatting $PART as FAT32..."
mkfs.fat -F 32 -n ZKOS $PART > /dev/null

# Copy zkos binary
echo "[5/5] Copying zkos-minimal.bin..."
MOUNT=$(mktemp -d)
mount $PART $MOUNT
cp $ZKOS_BIN $MOUNT/zkos-minimal.bin
sync
umount $MOUNT
rmdir $MOUNT

echo ""
echo "Done! On U-Boot run:"
echo "  fatload mmc 1:1 0x80400000 zkos-minimal.bin"
echo "  go 0x80400000"
