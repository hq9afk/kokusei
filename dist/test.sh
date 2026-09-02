#!/usr/bin/env bash

# Build and run the unit test suite locally.

set -e
cd "$(dirname "$0")/.."

./dist/build.sh

meson test -C build --print-errorlogs
