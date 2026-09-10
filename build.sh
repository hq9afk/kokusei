#!/usr/bin/env bash

set -euo pipefail
cd "$(dirname "$0")"

cmd_setup() {
	sudo pacman -Syu --needed base-devel meson ninja mesa wayland wayland-protocols libxkbcommon sdbus-cpp freetype2 fontconfig cairo pango harfbuzz glib2 libsecret libsodium polkit pipewire wireplumber curl libqalculate libxml2 md4c nlohmann-json libical jemalloc stb ffmpeg
}

cmd_build() {
	if [ -d build ]; then
		meson setup --prefix=/usr --reconfigure build
	else
		meson setup --prefix=/usr build
	fi
	ninja -C build -j "${KOKUSEI_BUILD_JOBS:-4}"
}

cmd_install() { cmd_build; sudo ninja -C build install; }
cmd_run() { kokusei kill || true; cmd_install; kokusei; }
cmd_test() { cmd_build; meson test -C build --print-errorlogs; }
cmd_uninstall() { sudo ninja -C build uninstall; }

main() {
	local cmd="${1:-build}"
	case "$cmd" in
		setup|build|install|run|test|uninstall) "cmd_$cmd" ;;
		*) echo "unknown command: $cmd" >&2; exit 2 ;;
	esac
}

main "$@"
