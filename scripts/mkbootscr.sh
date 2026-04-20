#!/bin/bash
set -e

BOOT_CMD=$1

if [ -z "$BOOT_CMD" ]; then
    echo "Usage: ./mkbootscr.sh <boot.cmd>"
    echo "Example: ./scripts/mkbootscr.sh uboot/boot.cmd"
    exit 1
fi

if [ ! -f "$BOOT_CMD" ]; then
    echo "Error: $BOOT_CMD not found"
    exit 1
fi

BOOT_SCR="${BOOT_CMD%.cmd}.scr"
mkimage -A arm64 -T script -C none -d "$BOOT_CMD" "$BOOT_SCR"
echo "Generated: $BOOT_SCR"
