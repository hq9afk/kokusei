# `kokusei` index

## Rule

- One-line, no break.
- Grouped by `directory`, one `##` heading per directory.
- Entry format: `file`: Purpose (≤ 20 words).
- Reflect current structure and function of each file in the code base.
- No mentions of past fixes.

## src/app

- `config.h`+`.cpp`: JSON config loader/saver with atomic write and inotify hot-reload.
- `single_instance_lock.h`+`.cpp`: `flock()`-based single-instance lock.
- `ipc.h`+`.cpp`: Kokusei's own control socket, client/server request handling; verb table from each module.
- `key_dispatch.h`+`.cpp`: Routes key events to whichever module owns the surface `KeyboardState::focused_surface` currently names, so `kokusei.cpp` never names a module's key handler.
- `monitor_output.h`+`.cpp`: `MonitorOutput` per-output state, monitor create/activate/destroy lifecycle, config-apply orchestration, trulla retarget.
- `module.h`: `Module` interface, the per-surface overlay boundary; default no-op virtuals, unnamed params; `apply_config` hook fired for every overlay on a config change.
- `per_monitor_module.h`: `PerMonitorModule` interface, the per-surface per-monitor boundary; default no-op virtuals, unnamed params.
- `module_registry.h`+`.cpp`: `build_app_modules`/`build_per_monitor_modules` composition root, one subclass per overlay/per-monitor surface; also the `penance_notify_output_*`/`penance_is_locked` bridge so `app/` code reaches the penance module without a module include.
- `wayland_registry.h`+`.cpp`: Wayland global registry bind/listener wiring, populates `WaylandState`'s globals; notifies the penance module of output hotplug.
- `wayland_state.h`: `WaylandState`, shared Wayland globals and every process-wide service's owned state; forward-declares `MonitorOutput`.
- `service.h`: `Service` interface, the process-wide boundary for cross-cutting services: `init`/`timer_tick`/`poll_sources`.
- `service_registry.h`+`.cpp`: `build_services` composition root, one `Service` subclass per cross-cutting service.
- `user_info.h`+`.cpp`: `getpwuid`-based username, `/etc/os-release` `PRETTY_NAME`, `sysinfo`-based uptime string, and `profile_media_path` resolution, shared across modules.
- `text_input_client.h`: `TextInputClient` interface, `TextInputState`/`TextInputEdit`, implemented by each field-owning module's wrapper class.

## src/config

- `qixing_config.h`: Qixing geometry, spacing, and pill-order constants.
- `overseer_config.h`: Every overseer data type and constant, no function bodies.
- `spark_config.h`: OSD surface size/margin/duration/animation-owner constants.
- `herald_config.h`: Herald card padding/size/timing constants.
- `starward_config.h`: Ring-menu geometry, entry/exit hold/slash/burst/implode timings, `thunder_burst` constants, `{8/3}` star step, animation owner ids, and the 8-button action table.
- `yuheng_config.h`: Yuheng card-stack geometry and gauge/temp-warn color constants.
- `liyue_config.h`: Liyue workspace-grid geometry, timing, and live-capture throttle constants.
- `expanse_config.h`: Expanse layer-shell namespace constant.
- `trulla_config.h`: Trulla panel layout, opacity, animation, group, widget, popup, spinner constants, `TrullaFieldId` enum, and the five-entry nav-rail tab table.
- `stiletto_config.h`: Stiletto-rain window size, glyph/cell/timing constants.
- `blink_config.h`: Blink recent-activity pulse and blink-overlay fade, logo-speed, and layer-namespace constants.
- `penance_config.h`: Penance-screen card ratio, three-column and side-panel geometry, fetch/media/resources/notification-dock constants, dot/input/avatar sizes, entrance/exit animation timings, and per-property animation owner ids.
- `resonance_config.h`: Audio resonance surface-derived square render canvas (`min(w,h)/2`), `0.7` black backdrop, `11 kHz` stereo capture, CPU FFT, GLava GPU-transform constants, plus `ResonanceParams` runtime knobs (fps, particle thin/size, fractal complexity, glow directions/quality) and their clamp ranges.

## src/render

- `palette.h`: `Color` struct, compile-time hex parser, and the full ported color/metrics palette.
- `color_ops.h`: Runtime `with_alpha`/`lerp_color` color operations for animations.
- `texture.h`+`.cpp`: RAII GL texture handle and creation/update helpers, RGBA or direct BGRA upload, plus `GL_EXTENSIONS` cap probes.
- `panel_scroll.h`+`.cpp`: Shared scroll-offset/clamp/wheel-input helper for scrollable panel lists.
- `text.h`+`.cpp`: Cairo+Pango text rasterization, fixed body/small/large font sizes, `rasterize_text_px` for one-off pixel sizes (penance-screen clock), `kokusei_text_advance` fixed monospace cell width, and `make_text_texture` string-to-`Texture`.
- `text_elide.h`+`.cpp`: Pango-free character-count string elision, end (`elide`) and middle (`elide_middle`); linked into the test binary.
- `marquee_scroll.h`+`.cpp`: `MarqueeTextState` and the pure pause/scroll/pause/snap loop state machine, driven by `AnimationManager`; no Node/texture dependency.
- `marquee_text.h`+`.cpp`: `draw_marquee_text`, the Node-drawing wrapper around `marquee_scroll.h`; animates a clipped scroll only when text overflows.
- `animated_image.h`+`.cpp`: `AnimatedImage` playable still/animated picture; wall-clock frame cycling over the `media_service` `.rgba` cache, `show`/`hide` releasing frame textures while off-screen, ring+circular-crop draw.
- `renderer.h`+`.cpp`: GL draw calls, clip-stack management, `Affine2D` model-transform stack, shared `Renderer` across every surface; `draw_custom` escape hatch runs a module-owned shader program over the shared quad with the common vertex uniforms set.
- `rect.h`: Shared `Rect{x,y,w,h}` struct for hit-testing.
- `panel_chrome.h`+`.cpp`: Shared box/header/confirm chrome, click-kind enum, `panel_region_hit`, `panel_draw_toggle_switch`, `panel_draw_centered_text`, and `panel_measure_row_actions`/`panel_draw_row_actions` (connect/forget pill or busy label) for on-demand panels.
- `node.h`+`.cpp`: `Node` retained-allocation scene graph with per-frame node pooling; kinds are rect/rounded-rect/texture/rounded-texture/video-texture/group; per-node `rotation`/`scale` about the node centre.
- `video_texture.h`+`.cpp`: `VideoTexture` RAII `EGLImageKHR`/`GL` handle plus `DrmFrameImport` dma-buf import for zero-copy `VAAPI` playback, and the `EGL_EXT_image_dma_buf_import` cap probe.
- `gl.h`+`.cpp`: Labelled shader compile/link helper (`gl_compile_program`) and `gl_check` `glGetError` drain, both logging through `klog`.
- `overlay_panel.h`+`.cpp`: Shared full-screen on-demand overlay surface, position-lock-on-toggle recipe, and `PanelHeightReveal` live-height roll-down/collapse helper.
- `toplevel_window.h`+`.cpp`: Shared `xdg_toplevel` real-window surface lifecycle for compositor-managed windows.
- `popup_window.h`+`.cpp`: Shared `xdg_popup` surface lifecycle parented to a layer surface via `zwlr_layer_surface_v1::get_popup`, with positioner, popup grab, `popup_done`, and reposition-on-resize.
- `layer_surface.h`+`.cpp`: Shared layer-shell surface creation helper, dedupes anchor/margin/listener setup.
- `scene.h`: Thin `Scene` holder over `node.h` - a root `Node` plus `dirty`/`draw`/`rebuild` one-liners; no scene-graph logic of its own.
- `image.h`+`.cpp`: JPEG/PNG/SVG decode (sniffed from content) and GL texture upload, no GIF; SVG rasterized via `librsvg`+Cairo.
- `texture_cache.h`+`.cpp`: Path-keyed decoded-texture cache built on `texture.h`.
- `icon.h`+`.cpp`: Direct FreeType+Cairo rendering of single icon glyphs, plus `make_icon_texture` glyph-to-`Texture`.
- `icons.h`: Tabler Icons codepoint constants.
- `text_field.h`+`.cpp`: Shared single-line editable text buffer core (key handling, UTF-8-safe backspace, compose/IME preedit) plus every shell input's shared animations: `text_field_blink_toggle`/`draw_text_field_caret` caret blink, `TextFieldTypeAnim`+`text_field_type_anim_sync`/`_settle`/`_clear` per-character `EaseOutBack` pop, `TextFieldRowSlide`+`text_field_row_slide`/`_reset` `Behavior`-on-`x` whole-row slide for centered fields whose origin moves with length, and `draw_text_field_value` fixed-advance per-glyph value draw; `draw_text_field_preedit`.
- `animation.h`+`.cpp`: `AnimationManager`, wall-clock tween/easing engine, owner-tag auto-cancel.
- `slider.h`+`.cpp`: `draw_slider_track`, shared track+fill+click-region drawing for any slider.
- `arc_gauge.h`+`.cpp`: `cached_arc_gauge`/`draw_arc_gauge`, shared cached circular arc-gauge texture plus icon/value/optional-sub-label stack layout; diameter, stroke, gaps, and tint colors are all caller params. Consumed by `yuheng`'s system-stats card and `penance`'s resource gauges.
- `progress_bar.h`+`.cpp`: `draw_flat_bar`, shared track+fill rounded-bar drawing with a caller-set minimum fill width; no click regions or panel dependency. Consumed by `battery_panel`, `system_monitor_panel`, and `spark`'s OSD.
- `stiletto_grid.h`+`.cpp`: Stiletto-rain glyph column simulation and Cairo-rasterized texture, state-free of surface concerns.

## src/service

- `bluetooth_service.h`+`.cpp`: BlueZ D-Bus client, device-classification logic, rfkill soft-block reader/clearer.
- `brightness_service.h`+`.cpp`: Backlight `sysfs` reader and `inotify` watch, plus `brightness_set` via a `brightnessctl` subprocess; shared by the OSD service and yuheng.
- `network_service.h`+`.cpp`: NetworkManager client (nmcli subprocesses + D-Bus) and pure output parsers.
- `notification_service.h`+`.cpp`: `org.freedesktop.Notifications` D-Bus server, `NotificationRecord` store (`Notify` add/replace, `CloseNotification` erase), and the expiry sweep; consumed by the `herald` renderer and the `penance` lock dock.
- `tray_service.h`+`.cpp`: StatusNotifierWatcher/host implementation and DBusMenu tree fetch.
- `mpris_service.h`+`.cpp`: Minimal MPRIS client, async player scan and selection policy, transport control methods.
- `upower_service.h`+`.cpp`: UPower D-Bus client; single display device for the qixing's battery pill, plus full device enumeration for the battery panel.
- `pipewire_service.h`+`.cpp`: Direct libpipewire client for OSD volume/mic triggers and volume-panel writes; also `DraggedSlider`, tag-to-node-id resolution, and drag-to-volume application.
- `telemetry_service.h`+`.cpp`: CPU/GPU temperature and usage via hwmon/thermal-zone/`nvidia-smi`, plus CPU frequency, CPU/RAM/disk usage, and network throughput.
- `frame_service.h`+`.cpp`: Frame-callback paint pacing shared across surfaces; first paint synchronous, later repaints deferred to `frame_done`.
- `input_service.h`+`.cpp`: all `wl_seat` input - `wl_keyboard`+xkbcommon (key-repeat, compose key, modifiers, `focused_surface` tracking `key_dispatch.cpp` routes on), `wl_pointer` (hover, click queue with button and click-time coords, cursor-shape), and the shared seat-capabilities listener.
- `text_input_service.h`+`.cpp`: `zwp_text_input_v3` client-role protocol glue for IME composition (fcitx5/ibus), focus tracking, preedit/commit/delete dispatch to the active `TextInputClient`.
- `hyprland_service.h`+`.cpp`: Hyprland IPC client: per-monitor workspace/monitor/client state via request+event sockets, plus `hypr_tile_*` tiling actions dispatched as `hl.dsp.*` Lua calls; header also owns the `Workspace`/`MonitorWorkspaces` types.
- `capture_service.h`+`.cpp`: Per-window `hyprland-toplevel-export-v1` live capture; `wl_shm` buffer alloc/reuse and GL texture upload, throttled per window.
- `output_service.h`+`.cpp`: Pure-data `Output` struct plus output-selection logic, and per-output fractional-scale listener tracking (`OutputScale`).
- `expanse_service.h`+`.cpp`: Per-monitor, per-column expanse path/count/fill-mode resolution; a `bool animated` selects the static or animated config maps.
- `media_service.h`+`.cpp`: The shell's one media decoder, host side. Loads the `media_plugin` via `dlopen`; owns the `AnimateJob` async `.rgba` frame cache (`.tmp`-renamed, `fps * 30 s` ceiling), `animate_decode_scaled` still/first-frame decode+downscale+cache, and the `animate_scale_filter`/`animate_decode_size`/`kAnimate*`/`animate_frame_index` policy. Test-linked; no `libav`.
- `media_plugin.h`+`.cpp`: The `shared_module` that links `libavcodec`/`libavfilter`, isolated so a missing/mismatched `ffmpeg` only disables animated content. Does the actual decoding: paced/looping/hw-accel/zero-copy playback for animated expanse (behind `media_decode_stream`), and a software-only bounded `RGBA` frame-set for UI gifs (behind `media_decode_frames`). Header declares the three `extern "C"` entry points the loader reaches via `dlsym`.
- `trulla_service.h`+`.cpp`: Trulla field-text parsing into `Config` and the config-save wrapper.
- `icon_service.h`+`.cpp`: App icon path resolution across GTK icon themes (Adwaita, breeze, hicolor), PNG then SVG, via a cached index.

## src/core

- `deferred_call.h`+`.cpp`: Cross-thread callback hand-off so worker threads can post to the main thread.
- `log.h`+`.cpp`: `klog()` dual stderr + logfile logging with timestamps.
- `path_home.h`+`.cpp`: `path_collapse_home`/`path_expand_home` `$HOME` <-> `~` path rewriters, shared by `config`, `trulla`, and `overseer`.
- `sdbus_poll_source.h`: Wraps an sdbus connection's poll data into a `PollSource`.
- `poll_source.h`+`.cpp`: `PollSource` interface and `FnPollSource` helper for the main poll loop.
- `async_process.h`+`.cpp`: Worker-thread subprocess runner, plus `spawn_detached` for fire-and-forget commands.

## src/modules

- `qixing.h`+`.cpp`: Qixing rendering, autohide geometry, pill-click dispatch, qixing surface's own EGL; shared `WaylandState`-wide helpers.
- `overseer.h`+`.cpp`: `OverseerState`, surface/EGL/tick/toggle/key/click/pointer-hover/paint core only.
- `spark.h`+`.cpp`: Volume/brightness popup, per-monitor, auto-hides, reactive to system state changes.
- `herald.h`+`.cpp`: Notification renderer; `herald_sync` rebuilds `HeraldEntry` render/animation state by `id` from `notification_service` records, per-monitor `HeraldView` card paint, and per-card `x` close button that dismisses only on the clicked monitor via a per-view fade.
- `starward.h`+`.cpp`: Starward (logout ring) state, rasterizer, Keqing-burst entry/exit choreography (logo pop, hold, eight fast overshooting `thunder_burst` slashes along the `{8/3}` star path, hold, a `thunder_shock` shockwave plus one big left-to-right upward-slanted finishing slash that push the buttons out together; close runs the same slashes, hold, and shockwave/finishing slash but the buttons spread further out while fading and the logo just fades, no zoom), config-driven static/animated `AnimatedImage` centre logo, input dispatch, and paint.
- `yuheng.h`+`.cpp`: Yuheng singleton state, fixed top-right overlay, IPC/widget-triggered open wiring, clamped/scrollable card layout, brightness card driving `brightness_set`, `AnimatedImage` profile avatar freed while closed.
- `liyue.h`+`.cpp`: Liyue state, Hyprland-only full-screen exclusive-keyboard overlay; paginated workspace grid, live per-window `hyprland-toplevel-export-v1` thumbnails, click/drag/keyboard focus-move-swap-close, IPC-only toggle.
- `expanse.h`+`.cpp`: Per-monitor background surface; defines `ExpanseColumn` (one column's static `Texture` or animated `media_service` playback + zero-copy `VideoTexture`, generation-guarded upload, fill-mode draw) and drives a `ExpanseColumn` vector synced from config, drawn shared by `penance` and blink ambient. `ExpansePerMonitorModule::tick` clears `ExpanseState::visible` and pauses column decode while a fullscreen overlay (`starward`/`liyue`) covers the output; `expanse_paint` no-ops while `!visible` or `session_locked`, but decode keeps running under a lock so `penance` composites the live wallpaper.
- `blink.h`+`.cpp`: Recent-activity blink clock feeding the per-monitor ambient/screensaver overlay surface; screensaver bounces an `AnimatedImage` logo, freed while not shown.
- `trulla.h`+`.cpp`: Trulla panel core, hosts per-tab modules, responsive nav rail, owns shared toggle-row widgets and `draw_profile_block`.
- `stiletto.h`+`.cpp`: Stiletto-rain overlay, a real `xdg_toplevel` window, rebuilds the grid on live resize.
- `resonance.h`+`.cpp`: Audio resonance overlay window; ported `ncs`/WayVes Perlin-noise blob (tinted to `accent` over a `0.7` black backdrop) plus `glow` post pass, fed by own `11 kHz` stereo PipeWire capture, CPU FFT, and a GLava GPU transform chain; dedicated render thread and share-context `EGLContext`; render thread self-paces to `ResonanceParams::fps` and reads live knobs via `resonance_apply_params` (`Module::apply_config`).
- `penance.h`+`.cpp`: `ext-session-lock-v1` session lock; one lock surface per `wl_output`, `PAM` auth on a worker thread, `caelestia`-style fixed-ratio card with a three-column layout (battery/fetch/media, center clock+date+avatar+pill, resources/notifications) drawn from `mpris`/`system_stats`/`cpu_temp`/`gpu_temp`/`upower`/`notification_service`, entrance/exit spin-expand choreography.

## src/modules/starward

- `thunder_burst.h`+`.cpp`: Two single-pass `GLES2` fragment effects reached through `Renderer::draw_custom`, each with its own lazily-compiled program, bypassing the `Node`/`Scene` graph: `thunder_burst_draw` (`kThunderBurstFs`) a forked-lightning bolt scoped to its segment bbox for the eight star-path slashes and the finishing slash; `thunder_shock_draw` (`kThunderShockFs`) a radial shockwave ring for the button-push burst.

## src/modules/resonance

- `fft.h`+`.cpp`: Radix-2 DIT FFT (`GLava`-derived, GPL-3.0), Hann window plus `log`/`fftScale`/`fftCutOff` magnitude tilt; `EGL`-free, linked into the test binary.
- `audio_capture.h`+`.cpp`: Own `pw_thread_loop` `11 kHz` stereo sink capture; `ncs` ring/fragment bookkeeping into `4096`-sample L/R buffers, `take()` snapshot under a mutex.
- `audio_stages.h`+`.cpp`: Render-thread GLava GPU transform chain (`pass` peak-hold + `gravity` decay -> 5-frame ring -> Hann `average` -> frequency-domain `smooth`) over `Nx1` `GL_R16` textures, for L and R.
- `blob_pipeline.h`+`.cpp`: Render-thread `ncs-1` (atomic-image particle accumulation) -> `ncs-2` (blob resolve) -> `glow` post, at a square canvas of `resonance_canvas_size(surfaceW, surfaceH)` rebuilt (atomic texture + three `RGBA8` FBOs) whenever the surface size changes, with `glMemoryBarrier` between stages, `u_fade`/`u_accent`/`u_backdrop` plus the `ResonanceParams` knob uniforms (`particleThin`/`u_particleSize`/`u_complexity`/`u_glowDirections`/`u_glowQuality`), and a centered `glBlitFramebuffer` compositing the canvas onto the window surface.
- `resonance_shaders.h`: The eight flattened `ncs` shader stages as string fragments (includes inlined, `#expand` hand-expanded), assembled at runtime.

## src/modules/penance

- `layout.h`+`.cpp`: Pure penance-panel geometry math (fixed-ratio card size, three-column split, equal-height side-card split, center scale, content-stack height, fetch colour-box count, dot row); no `EGL`, linked into the test binary.
- `pam_authenticator.h`+`.cpp`: `pam_start_confdir`-based password check for the current user against the shipped `kokusei` `PAM` service, with a `login` fallback; run off the poll thread by the penance module.

## src/modules/overseer

- `apps_provider.h`+`.cpp`: App name scoring and search over `DesktopEntry` lists.
- `desktop_entry.h`+`.cpp`: `.desktop` file parsing, scanning, and launch dispatch.
- `files_provider.h`+`.cpp`: Path scoring and `fd`-backed file/dir search.
- `launch_action.h`+`.cpp`: URL/run/web-search launch dispatch.
- `search.h`+`.cpp`: Query-mode prefix detection and combined result ranking.
- `submenu.h`+`.cpp`: Directory-browse submenu navigation and entry actions.
- `visit_store.h`+`.cpp`: Per-item launch-count persistence for result ranking.

## src/modules/trulla

- `expanse_tab.h`+`.cpp`: Per-tab trulla UI and commit logic.
- `displays_tab.h`+`.cpp`: Per-tab trulla UI and commit logic.
- `blink_tab.h`+`.cpp`: Per-tab trulla UI and commit logic.
- `starward_tab.h`+`.cpp`: Per-tab trulla UI and commit logic (central-logo static/animated toggle).
- `resonance_tab.h`+`.cpp`: Per-tab trulla UI and commit logic; number-field row per `ResonanceParams` knob with per-row reset.

## src/modules/qixing

- `panel/network_panel.h`+`.cpp`: One `<name>_panel.h/.cpp` pair per on-demand panel.
- `panel/bluetooth_panel.h`+`.cpp`: One `<name>_panel.h/.cpp` pair per on-demand panel.
- `panel/volume_panel.h`+`.cpp`: One `<name>_panel.h/.cpp` pair per on-demand panel.
- `panel/tray_panel.h`+`.cpp`: On-demand tray grid panel plus the tray menu, rendered into a separate `xdg_popup` (`popup_window`) grabbed to the panel layer surface; `menu_path` navigation, reposition on level change.
- `panel/battery_panel.h`+`.cpp`: One `<name>_panel.h/.cpp` pair per on-demand panel.
- `panel/system_monitor_panel.h`+`.cpp`: One `<name>_panel.h/.cpp` pair per on-demand panel.
- `panel/clock_panel.h`+`.cpp`: On-demand centered month-grid calendar panel; header prev/today/next month nav, weekday row, `6x7` day grid with today highlighted.
- `widget/widget_capsule.h`+`.cpp`: Shared pill bookkeeping, hover-expand/click dispatch, and the pill-row layout/draw.
- `widget/workspace_widget.h`+`.cpp`: Workspace-row drawing plus trailing liyue-toggle icon; records per-pill and icon hit rects for click routing.
- `widget/clock_widget.h`+`.cpp`: State-free clock-pill drawing; returns the pill hit rect and owns the calendar-panel open trigger.
- `widget/starward_widget.h`+`.cpp`: One pair per qixing pill.
- `widget/battery_widget.h`+`.cpp`: One pair per qixing pill.
- `widget/network_widget.h`+`.cpp`: One pair per qixing pill.
- `widget/bluetooth_widget.h`+`.cpp`: One pair per qixing pill.
- `widget/volume_widget.h`+`.cpp`: One pair per qixing pill.
- `widget/yuheng_widget.h`+`.cpp`: One pair per qixing pill.
- `widget/system_monitor_widget.h`+`.cpp`: One pair per qixing pill (CPU pill opening the system-monitor panel).
- `widget/tray_widget.h`+`.cpp`: One pair per qixing pill (tray pill opening the tray panel).

## src/shaders

- `renderer_shaders.h`: The shared `Renderer`'s `#version 320 es` shaders (quad vertex; rect/tex/rrect/rounded-tex/video fragment) plus `starward`'s two `thunder` fragment shaders.

## src

- `kokusei.cpp`: Orchestration, Wayland/EGL bootstrap, poll loop, CLI entry point, daemonize/debug/IPC-client dispatch.

## test

`test/`: One test file per pure-logic header, grouped by module, run through one `kokusei-test` binary via meson.

- kokusei-test.cpp
- kokusei-test.hpp
- app/test_config.cpp
- app/test_expanse_resolve.cpp
- core/test_async_process.cpp
- core/test_deferred_call.cpp
- core/test_path_home.cpp
- core/test_poll_source.cpp
- dbus/test_network_parse.cpp
- dbus/test_bluetooth.cpp
- overseer/test_overseer.cpp
- wayland/test_keyboard.cpp
- wayland/test_active_output.cpp
- system/test_rfkill.cpp
- render/test_animation.cpp
- render/test_animated_image.cpp
- render/test_marquee_scroll.cpp
- render/test_palette.cpp
- render/test_image_decode.cpp
- render/test_text_elide.cpp
- penance/test_layout.cpp
- resonance/test_fft.cpp
- dbus/test_mpris.cpp
- system/test_cpu_temp.cpp
- system/test_gpu_temp.cpp
- system/test_system_stats.cpp

## root

- `meson.build`: Build config, dependency list, test registration.
- `convention.md`: Formatting and commenting rules.

## dist

- `build.sh`: Shared configure+compile step (RAM-capped job count via `KOKUSEI_BUILD_JOBS`), called by `test.sh` and `install.sh`.
- `{run,install,test}`: Convenience scripts to build+test, build+install, or kill+install+launch kokusei.

## assets

- `fonts/*`, `bullets/*`: Installed fonts, overseer bullet icons.
- `default.png`: Default expanse wallpaper, the `KOKUSEI_DEFAULT_WALLPAPER` fallback when a column has no configured path.
- `default_wp.svg`: Blink screensaver bouncing-logo source (placeholder).
- `electro.png`: Password-field echo glyph, ported from `keqing-shell`'s `Input.qml`, drawn per character.
- `gifs/profile.gif`: Penance avatar, trulla and yuheng profile-picture source, decoded to cached frames via `ffmpeg`.
- `starward/logo.gif`: Starward animated centre-logo source, decoded to cached frames via `ffmpeg`.
- `starward/logo.png`: Starward static centre-logo source, used when the animated-logo toggle is off.
- `pam/kokusei`: `PAM` service file for the penance screen, loaded via `pam_start_confdir`.

## protocols

- `wlr-layer-shell-unstable-v1.xml`: Wayland protocol XML, code-generated at build time.
- `hyprland-toplevel-export-v1.xml`: Per-window live-capture protocol XML, code-generated, used by `liyue`.
- `text-input-unstable-v3.xml`: IME text-input protocol XML, code-generated, used by `text_input_service`.
- `wlr-foreign-toplevel-management-unstable-v1.xml`: Unused directly; linked only to satisfy a symbol `hyprland-toplevel-export-v1`'s v2 request references.
- `ext-session-lock-v1`: From `wayland-protocols` (`staging/`), code-generated at build time, used by `penance`.

## local

- `local/`: Planning/design docs, not part of the shipped repo.
