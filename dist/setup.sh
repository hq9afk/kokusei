#!/usr/bin/env bash

# Install dependencies

set -euo pipefail

sudo pacman -Syu --needed base-devel meson ninja mesa wayland wayland-protocols libxkbcommon sdbus-cpp freetype2 fontconfig cairo pango harfbuzz glib2 libsecret libsodium polkit pipewire wireplumber curl libqalculate libxml2 md4c nlohmann-json libical jemalloc stb ffmpeg
