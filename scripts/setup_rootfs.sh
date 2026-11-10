#!/bin/bash
set -e
cd "$(dirname "$0")/.."               

if [ -d alpine ] && [ -x alpine/bin/busybox ]; then
    echo "rootfs already exists in ./alpine"; exit 0
fi

ARCH=$(uname -m)                              # x86_64 or aarch64
BASE="https://dl-cdn.alpinelinux.org/alpine/latest-stable/releases/$ARCH"

echo "Looking up the latest Alpine minirootfs for $ARCH..."
FILE=$(curl -fsSL "$BASE/latest-releases.yaml" \
       | grep -o "alpine-minirootfs-[0-9.]*-$ARCH.tar.gz" | head -1)
[ -n "$FILE" ] || { echo "could not find a rootfs file at $BASE"; exit 1; }

echo "Downloading $FILE..."
mkdir -p alpine
curl -fsSL "$BASE/$FILE" | sudo tar -xz -C alpine
echo "rootfs ready in ./alpine"