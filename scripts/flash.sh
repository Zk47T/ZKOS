#!/bin/bash
set -e

FLASH_BIN=$1
ZKOS_BIN=$2
DEVICE=$3

if [ -z "$FLASH_BIN" ] || [ -z "$ZKOS_BIN" ] || [ -z "$DEVICE" ]; then
    echo "Usage: ./flash.sh <flash.bin> <zkos.bin> <device>"
    echo "Example: sudo ./scripts/flash.sh blob/flash.bin boot/2-uart-echo/zkos.bin /dev/mmcblk0"
    exit 1
fi

if [ ! -b "$DEVICE" ]; then
    echo "Error: $DEVICE is not a block device"
    exit 1
fi

if [ ! -f "$FLASH_BIN" ]; then
    echo "Error: $FLASH_BIN not found"
    exit 1
fi

if [ ! -f "$ZKOS_BIN" ]; then
    echo "Error: $ZKOS_BIN not found"
    exit 1
fi

REPO_DIR=$(dirname "$(realpath "$0")")/..

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

umount ${DEVICE}* 2>/dev/null || true

echo "[1/6] Clearing first 8MB..."
dd if=/dev/zero of=$DEVICE bs=1M count=8 status=none

echo "[2/6] Writing flash.bin at 32KB offset..."
dd if=$FLASH_BIN of=$DEVICE bs=1K seek=32 status=none

echo "[3/6] Creating FAT32 partition..."
sfdisk -q $DEVICE <<EOF
4M,,c
EOF

PART="${DEVICE}p1"
[ -b "$PART" ] || PART="${DEVICE}1"
echo "[4/6] Formatting $PART as FAT32..."
mkfs.fat -F 32 -n ZKOS $PART > /dev/null

echo "[5/6] Generating boot.scr..."
BOOT_CMD="$REPO_DIR/scripts/boot.cmd"
mkimage -A arm64 -T script -C none -d $BOOT_CMD $REPO_DIR/scripts/boot.scr > /dev/null

echo "[6/6] Copying files to FAT32..."
MOUNT=$(mktemp -d)
mount $PART $MOUNT
cp $ZKOS_BIN $MOUNT/zkos.bin
cp $REPO_DIR/scripts/boot.scr $MOUNT/boot.scr
sync
umount $MOUNT
rmdir $MOUNT

echo ""
echo "Done! Board will auto-boot ZKOS."
