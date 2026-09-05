project(
  'kokusei',
  'c',
  'cpp',
  version: '0.1.0',
  default_options: ['cpp_std=c++23', 'warning_level=3'],
)

wayland_client_dep = dependency('wayland-client')
wayland_egl_dep    = dependency('wayland-egl')
egl_dep            = dependency('egl')
glesv2_dep         = dependency('glesv2') # libGLESv2 exports the ES 3.2 entry points; the shell targets #version 320 es
cairo_dep          = dependency('cairo')
pangocairo_dep     = dependency('pangocairo')
nlohmann_json_dep  = dependency('nlohmann_json')
sdbus_cpp_dep      = dependency('sdbus-c++')
freetype2_dep      = dependency('freetype2')
libpipewire_dep    = dependency('libpipewire-0.3', include_type: 'system')
xkbcommon_dep      = dependency('xkbcommon')
pam_dep            = meson.get_compiler('cpp').find_library('pam')
libpng_dep         = dependency('libpng')
libjpeg_dep        = dependency('libjpeg')
librsvg_dep        = dependency('librsvg-2.0')
threads_dep        = dependency('threads')
libavformat_dep    = dependency('libavformat')
libavcodec_dep     = dependency('libavcodec')
libavfilter_dep    = dependency('libavfilter')
libavutil_dep      = dependency('libavutil')
# Only for DRM_FORMAT_* fourcc constants used in the zero-copy VAAPI dma-buf
# import; the VAAPI calls stay inside FFmpeg's own hwcontext_vaapi.
libdrm_dep         = dependency('libdrm')

kokusei_inc = include_directories('src')

kokusei_data_dir   = get_option('prefix') / get_option('datadir') / 'kokusei'
kokusei_font_dir   = kokusei_data_dir / 'fonts'
kokusei_bullet_dir = kokusei_data_dir / 'bullets'

install_data('assets/fonts/tabler-icons.ttf', install_dir: kokusei_font_dir) # shell-wide icons
install_data('assets/fonts/YujiMai.ttf', install_dir: kokusei_font_dir)      # starward module
install_data('assets/default.png', install_dir: kokusei_data_dir)            # default wallpaper
install_data('assets/default_wp.svg', install_dir: kokusei_data_dir)         # idle screensaver logo (placeholder)
install_data('assets/electro.png', install_dir: kokusei_data_dir)            # password echo glyph
install_data('assets/gifs/profile.gif', install_dir: kokusei_data_dir / 'gifs') # lock avatar + settings pfp
install_data('assets/starward/logo.gif', install_dir: kokusei_data_dir / 'starward') # starward centre logo (animated)
install_data('assets/starward/logo.png', install_dir: kokusei_data_dir / 'starward') # starward centre logo (static)
install_data('assets/pam/kokusei', install_dir: kokusei_data_dir / 'pam')    # lock PAM service
foreach n : ['1', '2', '3', '4', '5', '6']                                   # keqing constellations
  install_data('assets/bullets/' + n + '.png', install_dir: kokusei_bullet_dir)
endforeach

wayland_scanner = find_program('wayland-scanner')
wl_proto_dir = dependency('wayland-protocols').get_variable(pkgconfig: 'pkgdatadir')

# base => [xml, needs_sed]. The sed fixup renames the 'namespace' arg (a C++
# keyword) that wlr-layer-shell uses, matching noctalia's fix. Some protocols
# are listed only because another's requests reference their types and the
# generated code must still link: xdg-shell (layer-shell get_popup),
# tablet-unstable-v2 (cursor-shape), wlr-foreign-toplevel-management
# (toplevel-export v2) - nothing here calls them directly.
protocol_defs = [
  ['xdg-shell',                                   wl_proto_dir / 'stable/xdg-shell/xdg-shell.xml',                     false],
  ['wlr-layer-shell-unstable-v1',                 'protocols/wlr-layer-shell-unstable-v1.xml',                         true],
  ['ext-idle-notify-v1',                          wl_proto_dir / 'staging/ext-idle-notify/ext-idle-notify-v1.xml',     false],
  ['cursor-shape-v1',                             wl_proto_dir / 'staging/cursor-shape/cursor-shape-v1.xml',           false],
  ['tablet-unstable-v2',                          wl_proto_dir / 'unstable/tablet/tablet-unstable-v2.xml',             false],
  ['hyprland-toplevel-export-v1',                 'protocols/hyprland-toplevel-export-v1.xml',                         false],
  ['wlr-foreign-toplevel-management-unstable-v1', 'protocols/wlr-foreign-toplevel-management-unstable-v1.xml',         false],
  ['text-input-unstable-v3',                      'protocols/text-input-unstable-v3.xml',                              false],
  ['ext-session-lock-v1',                         wl_proto_dir / 'staging/ext-session-lock/ext-session-lock-v1.xml',   false],
]

sed_fix = ' && sed -i s/namespace/name_space/g "$3"'
proto = {}
foreach p : protocol_defs
  base = p[0]
  fixup = p[2] ? sed_fix : ''
  proto += {base: [
    custom_target(base + '-client-header',
      input: p[1], output: base + '-client-protocol.h',
      command: ['sh', '-c', '"$1" client-header "$2" "$3"' + fixup,
                '_', wayland_scanner.full_path(), '@INPUT@', '@OUTPUT@']),
    custom_target(base + '-client-source',
      input: p[1], output: base + '-client-protocol.c',
      command: ['sh', '-c', '"$1" private-code "$2" "$3"' + fixup,
                '_', wayland_scanner.full_path(), '@INPUT@', '@OUTPUT@']),
  ]}
endforeach

proto_gen_all = []
foreach _name, tgts : proto
  proto_gen_all += tgts
endforeach

# Test binary links only the protocols its sources actually reference.
proto_gen_test = [
  proto['wlr-layer-shell-unstable-v1'],
  proto['xdg-shell'],
  proto['cursor-shape-v1'],
  proto['tablet-unstable-v2'],
]

# .h/.cpp module pairs shared between the main executable and the test binary;
# header-only .hpp modules need no entry. *_test_sources are exercised by
# test/**; *_main_only_sources pull in EGL/GL/cairo/pango/pipewire or generated
# protocol code and have no test coverage.
kokusei_core_split_sources = [
  'src/core/log.cpp',
  'src/core/poll_source.cpp',
  'src/core/deferred_call.cpp',
  'src/core/async_process.cpp',
  'src/core/path_home.cpp',
]

kokusei_render_test_sources = [
  'src/render/animation.cpp',
  'src/render/marquee_scroll.cpp',
  'src/render/texture.cpp',
  'src/render/image.cpp',
  'src/render/text_elide.cpp',
]

kokusei_render_main_only_sources = [
  'src/render/gl.cpp',
  'src/render/renderer.cpp',
  'src/render/node.cpp',
  'src/render/text.cpp',
  'src/render/icon.cpp',
  'src/render/texture_cache.cpp',
  'src/render/panel_scroll.cpp',
  'src/render/text_field.cpp',
  'src/render/panel_chrome.cpp',
  'src/render/marquee_text.cpp',
  'src/render/animated_image.cpp',
  'src/render/overlay_panel.cpp',
  'src/render/toplevel_window.cpp',
  'src/render/popup_window.cpp',
  'src/render/layer_surface.cpp',
  'src/render/slider.cpp',
  'src/render/arc_gauge.cpp',
  'src/render/progress_bar.cpp',
  'src/render/stiletto_grid.cpp',
  'src/render/video_texture.cpp',
]

kokusei_service_test_sources = [
  'src/service/bluetooth_service.cpp',
  'src/service/brightness_service.cpp',
  'src/service/network_service.cpp',
  'src/service/tray_service.cpp',
  'src/service/upower_service.cpp',
  'src/service/mpris_service.cpp',
  'src/service/telemetry_service.cpp',
  'src/service/input_service.cpp',
  'src/service/output_service.cpp',
  'src/service/expanse_service.cpp',
  'src/service/media_service.cpp',
  'src/service/trulla_service.cpp',
  'src/service/icon_service.cpp',
]

kokusei_service_main_only_sources = [
  'src/service/pipewire_service.cpp',
  'src/service/frame_service.cpp',
  'src/service/hyprland_service.cpp',
  'src/service/capture_service.cpp',
  'src/service/text_input_service.cpp',
  'src/service/notification_service.cpp',
]

kokusei_overseer_main_only_sources = [
  'src/modules/overseer.cpp',
]

kokusei_overseer_test_sources = [
  'src/modules/overseer/apps_provider.cpp',
  'src/modules/overseer/desktop_entry.cpp',
  'src/modules/overseer/files_provider.cpp',
  'src/modules/overseer/launch_action.cpp',
  'src/modules/overseer/search.cpp',
  'src/modules/overseer/submenu.cpp',
  'src/modules/overseer/visit_store.cpp',
]

kokusei_qixing_test_sources = []

kokusei_qixing_panels_main_only_sources = [
  'src/modules/qixing/panel/tray_panel.cpp',
  'src/modules/qixing/panel/network_panel.cpp',
  'src/modules/qixing/panel/bluetooth_panel.cpp',
  'src/modules/qixing/panel/volume_panel.cpp',
  'src/modules/qixing/panel/battery_panel.cpp',
  'src/modules/qixing/panel/system_monitor_panel.cpp',
  'src/modules/qixing/panel/clock_panel.cpp',
]

kokusei_qixing_main_only_sources = [
  'src/modules/qixing.cpp',
  'src/modules/qixing/widget/widget_capsule.cpp',
  'src/modules/qixing/widget/clock_widget.cpp',
  'src/modules/qixing/widget/workspace_widget.cpp',
  'src/modules/qixing/widget/battery_widget.cpp',
  'src/modules/qixing/widget/starward_widget.cpp',
  'src/modules/qixing/widget/bluetooth_widget.cpp',
  'src/modules/qixing/widget/network_widget.cpp',
  'src/modules/qixing/widget/yuheng_widget.cpp',
  'src/modules/qixing/widget/system_monitor_widget.cpp',
  'src/modules/qixing/widget/tray_widget.cpp',
  'src/modules/qixing/widget/volume_widget.cpp',
]

kokusei_blink_main_only_sources = [
  'src/modules/blink.cpp',
]

kokusei_penance_test_sources = [
  'src/modules/penance/layout.cpp',
]

kokusei_penance_main_only_sources = [
  'src/modules/penance.cpp',
  'src/modules/penance/pam_authenticator.cpp',
]

kokusei_overlay_main_only_sources = [
  'src/modules/spark.cpp',
  'src/modules/expanse.cpp',
  'src/modules/herald.cpp',
  'src/modules/starward.cpp',
  'src/modules/starward/thunder_burst.cpp',
  'src/modules/yuheng.cpp',
  'src/modules/liyue.cpp',
  'src/modules/stiletto.cpp',
  'src/modules/resonance.cpp',
  'src/modules/resonance/audio_capture.cpp',
  'src/modules/resonance/audio_stages.cpp',
  'src/modules/resonance/blob_pipeline.cpp',
]

kokusei_resonance_test_sources = [
  'src/modules/resonance/fft.cpp',
]

kokusei_app_test_sources = [
  'src/app/config.cpp',
]

kokusei_app_main_only_sources = [
  'src/app/single_instance_lock.cpp',
  'src/app/ipc.cpp',
  'src/app/key_dispatch.cpp',
  'src/app/module_registry.cpp',
  'src/app/monitor_output.cpp',
  'src/app/service_registry.cpp',
  'src/app/user_info.cpp',
  'src/app/wayland_registry.cpp',
]

kokusei_trulla_main_only_sources = [
  'src/modules/trulla.cpp',
  'src/modules/trulla/expanse_tab.cpp',
  'src/modules/trulla/displays_tab.cpp',
  'src/modules/trulla/blink_tab.cpp',
  'src/modules/trulla/starward_tab.cpp',
  'src/modules/trulla/resonance_tab.cpp',
]

kokusei_split_sources = [
  kokusei_core_split_sources,
  kokusei_render_test_sources,
  kokusei_render_main_only_sources,
  kokusei_service_test_sources,
  kokusei_service_main_only_sources,
  kokusei_overseer_test_sources,
  kokusei_overseer_main_only_sources,
  kokusei_blink_main_only_sources,
  kokusei_penance_test_sources,
  kokusei_penance_main_only_sources,
  kokusei_overlay_main_only_sources,
  kokusei_resonance_test_sources,
  kokusei_app_test_sources,
  kokusei_app_main_only_sources,
  kokusei_trulla_main_only_sources,
  kokusei_qixing_test_sources,
  kokusei_qixing_panels_main_only_sources,
  kokusei_qixing_main_only_sources,
]

kokusei_test_split_sources = [
  kokusei_core_split_sources,
  kokusei_render_test_sources,
  kokusei_service_test_sources,
  kokusei_overseer_test_sources,
  kokusei_penance_test_sources,
  kokusei_qixing_test_sources,
  kokusei_resonance_test_sources,
  kokusei_app_test_sources,
]

# ffmpeg's SONAME isn't stable across releases (see readme.md), so the only
# code that touches it is a dlopen'd plugin: a missing/mismatched ffmpeg then
# only disables animated content (loader falls back gracefully) instead of
# blocking startup.
media_plugin = shared_module(
  'kokusei-media',
  'src/service/media_plugin.cpp',
  'src/core/log.cpp', # klog() isn't exported by the host (no -rdynamic); the plugin needs its own copy
  include_directories: kokusei_inc,
  dependencies: [
    libavformat_dep,
    libavcodec_dep,
    libavfilter_dep,
    libavutil_dep,
    libdrm_dep,
  ],
  install: true,
  install_dir: kokusei_data_dir / 'plugins',
)

executable(
  'kokusei',
  'src/kokusei.cpp',
  kokusei_split_sources,
  proto_gen_all,
  include_directories: kokusei_inc,
  dependencies: [
    wayland_client_dep,
    wayland_egl_dep,
    egl_dep,
    glesv2_dep,
    cairo_dep,
    pangocairo_dep,
    nlohmann_json_dep,
    sdbus_cpp_dep,
    freetype2_dep,
    libpipewire_dep,
    xkbcommon_dep,
    threads_dep,
    libpng_dep,
    libjpeg_dep,
    librsvg_dep,
    libdrm_dep,
    pam_dep,
  ],
  cpp_args: [
    '-DKOKUSEI_FONT_DIR="' + kokusei_font_dir + '"',
    '-DKOKUSEI_DEFAULT_WALLPAPER="' + kokusei_data_dir / 'default.png' + '"',
    '-DKOKUSEI_BULLET_DIR="' + kokusei_bullet_dir + '"',
    '-DKOKUSEI_YUJIMAI_FONT="' + kokusei_font_dir / 'YujiMai.ttf' + '"',
    '-DKOKUSEI_STARWARD_LOGO="' + kokusei_data_dir / 'starward/logo.gif' + '"',
    '-DKOKUSEI_STARWARD_LOGO_STATIC="' + kokusei_data_dir / 'starward/logo.png' + '"',
    '-DKOKUSEI_IDLE_LOGO="' + kokusei_data_dir / 'default_wp.svg' + '"',
    '-DKOKUSEI_INPUT_ECHO="' + kokusei_data_dir / 'electro.png' + '"',
    '-DKOKUSEI_PAM_DIR="' + kokusei_data_dir / 'pam' + '"',
    '-DKOKUSEI_PROFILE_MEDIA="' + kokusei_data_dir / 'gifs/profile.gif' + '"',
    '-DKOKUSEI_MEDIA_PLUGIN="' + kokusei_data_dir / 'plugins/libkokusei-media.so' + '"',
  ],
  install: true,
  install_dir: 'bin',
)

kokusei_test = executable(
  'kokusei-test',
  'test/kokusei-test.cpp',
  kokusei_test_split_sources,
  proto_gen_test,
  'test/app/test_config.cpp',
  'test/app/test_expanse_resolve.cpp',
  'test/core/test_async_process.cpp',
  'test/core/test_deferred_call.cpp',
  'test/core/test_path_home.cpp',
  'test/core/test_poll_source.cpp',
  'test/dbus/test_network_parse.cpp',
  'test/dbus/test_bluetooth.cpp',
  'test/dbus/test_mpris.cpp',
  'test/overseer/test_overseer.cpp',
  'test/wayland/test_keyboard.cpp',
  'test/wayland/test_active_output.cpp',
  'test/system/test_rfkill.cpp',
  'test/system/test_cpu_temp.cpp',
  'test/system/test_gpu_temp.cpp',
  'test/system/test_system_stats.cpp',
  'test/render/test_animation.cpp',
  'test/render/test_animated_image.cpp',
  'test/render/test_marquee_scroll.cpp',
  'test/render/test_palette.cpp',
  'test/render/test_image_decode.cpp',
  'test/render/test_text_elide.cpp',
  'test/penance/test_layout.cpp',
  'test/resonance/test_fft.cpp',
  include_directories: kokusei_inc,
  dependencies: [
    threads_dep,
    xkbcommon_dep,
    wayland_client_dep,
    wayland_egl_dep,
    egl_dep,
    cairo_dep,
    pangocairo_dep,
    freetype2_dep,
    sdbus_cpp_dep,
    libpng_dep,
    libjpeg_dep,
    librsvg_dep,
    glesv2_dep,
    nlohmann_json_dep,
  ],
  cpp_args: [
    # Source asset, not the install dir, so the test runs uninstalled.
    '-DKOKUSEI_DEFAULT_WALLPAPER="' + meson.current_source_dir() / 'assets/default.png' + '"',
    '-DKOKUSEI_BULLET_DIR="' + kokusei_bullet_dir + '"',
    '-DKOKUSEI_FONT_DIR="' + kokusei_font_dir + '"',
    '-DKOKUSEI_MEDIA_PLUGIN="' + kokusei_data_dir / 'plugins/libkokusei-media.so' + '"',
  ],
)

test('kokusei', kokusei_test)
