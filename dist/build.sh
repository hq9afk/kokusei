#!/usr/bin/env bash

# Configure and compile the shell. RAM-capped via a bounded job count
# (override with KOKUSEI_BUILD_JOBS).

set -e
cd "$(dirname "$0")/.."

meson setup --prefix=/usr --reconfigure build
ninja -C build -j "${KOKUSEI_BUILD_JOBS:-4}"
