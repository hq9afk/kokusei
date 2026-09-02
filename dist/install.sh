#!/usr/bin/env bash

# Build and deploy the shell.

set -e
cd "$(dirname "$0")/.."

./dist/build.sh

sudo ninja -C build install
