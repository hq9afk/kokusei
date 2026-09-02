#include <GLES2/gl2.h>
#include <algorithm>
#include <cstdlib>

#include "core/deferred_call.h"
#include "core/log.h"
#include "core/path_home.h"

#include "modules/overseer.h"
#include "modules/overseer/apps_provider.h"
#include "modules/overseer/desktop_entry.h"
#include "modules/overseer/files_provider.h"
#include "modules/overseer/launch_action.h"
#include "modules/overseer/search.h"
#include "modules/overseer/submenu.h"
#include "modules/overseer/visit_store.h"

#include "render/icon.h"
#include "render/icons.h"
#include "render/image.h"
#include "render/layer_surface.h"
#include "render/node.h"
#include "render/palette.h"
#include "render/text_field.h"

#include "service/icon_service.h"

namespace {

void overseer_layer_surface_configure(void *data,
                                      zwlr_layer_surface_v1 *layer_surface,
                                      uint32_t serial, uint32_t width,
                                      uint32_t height) {
    auto *state = static_cast<OverseerState *>(data);
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    state->width = static_cast<int32_t>(width);
    state->height = static_cast<int32_t>(height);
    int32_t scale = state->output_scale.scale;
    if (state->egl_window)
        wl_egl_window_resize(state->egl_window, state->width * scale,
                             state->height * scale, 0, 0);
    state->configured = true;
}

void overseer_layer_surface_closed(void *, zwlr_layer_surface_v1 *) {}

constexpr zwlr_layer_surface_v1_listener overseer_layer_surface_listener = {
    .configure = overseer_layer_surface_configure,
    .closed = overseer_layer_surface_closed,
};

void overseer_update_input_region(OverseerState &state) {
    if (state.open) {
        wl_surface_set_input_region(state.surface, nullptr);
        return;
    }
    wl_region *empty_region = wl_compositor_create_region(state.compositor);
    wl_surface_set_input_region(state.surface, empty_region);
    wl_region_destroy(empty_region);
}

std::vector<FileEntry> overseer_dir_lister(const std::string &path,
                                           bool want_dirs) {
    return run_fd_search("", path, want_dirs, 50, 1, false);
}

void overseer_search_start_now(OverseerState &state) {
    state.search_query = state.effective_query;
    std::string pattern = to_glob_pattern(state.search_query);
    state.search_started_at = std::chrono::steady_clock::now();
    pid_t dirs_pid = async_process_start(
        state.search_dirs_proc,
        fd_search_argv(pattern, state.search_root, true, kOverseerMaxResults));
    pid_t files_pid = async_process_start(
        state.search_files_proc,
        fd_search_argv(pattern, state.search_root, false, kOverseerMaxResults));
    klog("overseer: search_start query='%s' dirs_pid=%d files_pid=%d",
         state.search_query.c_str(), dirs_pid, files_pid);
    state.search_running = true;
}

void overseer_search_start(OverseerState &state) {
    if (state.awaiting_restart) {
        state.pending_restart_query = state.effective_query;
        return;
    }
    if (state.search_running) {
        auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - state.search_started_at)
                .count();
        klog("overseer: CANCELLING still-running search after %lldms, "
             "deferring restart",
             static_cast<long long>(ms));
        state.pending_kill_dirs = async_process_cancel(state.search_dirs_proc);
        state.pending_kill_files =
            async_process_cancel(state.search_files_proc);
        state.pending_kill_since = std::chrono::steady_clock::now();
        state.pending_restart_query = state.effective_query;
        state.search_running = false;
        state.awaiting_restart = true;
        return;
    }
    overseer_search_start_now(state);
}

void overseer_query_changed(OverseerState &state) {
    ModeQuery mq = detect_mode_and_query(state.search.text);
    state.mode = mq.mode;
    state.effective_query = mq.query;
    state.search_dirty = true;
    state.search_dirty_at = std::chrono::steady_clock::now();
    state.search.cursor_blink_visible = true;
    text_field_type_anim_sync(state.query_anim, state.animations,
                              kOverseerQueryCharOwnerBase, state.search.text);
}

void overseer_query_char_clear(OverseerState &state) {
    text_field_type_anim_clear(state.query_anim, state.animations,
                               kOverseerQueryCharOwnerBase);
}

void overseer_launch_selected(OverseerState &state) {
    if (state.submenu.screen != SubmenuScreen::Search) {
        if (state.selected_index < 0 ||
            state.selected_index >=
                static_cast<int>(state.submenu.items.size()))
            return;
        SubmenuEntry entry = state.submenu.items[state.selected_index];
        if (submenu_handle_entry(state.submenu, entry, overseer_dir_lister)) {
            state.selected_index = 0;
            return;
        }
        launch_submenu_action(entry, state.visits);
        overseer_toggle(state, false);
        return;
    }

    if (state.mode != OverseerMode::Drun) {
        launch_non_drun(state.mode, state.effective_query);
        overseer_toggle(state, false);
        return;
    }

    if (state.selected_index < 0 ||
        state.selected_index >= static_cast<int>(state.results.size()))
        return;
    const DrunResult &r = state.results[state.selected_index];
    switch (r.kind) {
    case DrunResult::Kind::App:
        launch_drun_app(*r.app, state.visits);
        overseer_toggle(state, false);
        break;
    case DrunResult::Kind::Dir:
        submenu_open_directory(state.submenu, r.file.path, overseer_dir_lister);
        state.selected_index = 0;
        break;
    case DrunResult::Kind::File:
        submenu_open_file_actions(state.submenu, r.file.path);
        state.selected_index = 0;
        break;
    }
}

} // namespace

bool overseer_create_surface(OverseerState &state, wl_compositor *compositor,
                             zwlr_layer_shell_v1 *layer_shell,
                             wl_output *output) {
    state.compositor = compositor;
    LayerSurfaceConfig cfg{
        .layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        .name_space = "kokusei-overseer",
        .anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                  ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
    };
    state.layer_surface =
        layer_surface_create(state.surface, compositor, layer_shell, cfg,
                             &overseer_layer_surface_listener, &state, output);
    if (!state.layer_surface)
        return false;
    state.output_scale.on_change = [&state](int32_t scale) {
        if (state.egl_window)
            wl_egl_window_resize(state.egl_window, state.width * scale,
                                 state.height * scale, 0, 0);
        if (state.frame_clock.surface)
            request_frame(state.frame_clock);
    };
    output_scale_watch(state.output_scale, state.surface);
    overseer_update_input_region(state);
    wl_surface_commit(state.surface);

    state.visits = visit_store_load();
    return true;
}

bool overseer_init_egl(OverseerState &state, Renderer &renderer,
                       EGLDisplay display, EGLConfig config,
                       EGLContext context) {
    state.renderer = &renderer;
    state.egl_display = display;
    state.egl_context = context;
    int32_t scale = state.output_scale.scale;
    state.egl_window = wl_egl_window_create(state.surface, state.width * scale,
                                            state.height * scale);
    state.egl_surface = eglCreateWindowSurface(
        display, config,
        reinterpret_cast<EGLNativeWindowType>(state.egl_window), nullptr);
    if (state.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!eglMakeCurrent(display, state.egl_surface, state.egl_surface, context))
        return false;
    for (int i = 0; i < kOverseerMaxVisible; ++i)
        state.bullet_tex[i] = load_image_texture(
            KOKUSEI_BULLET_DIR "/" + std::to_string(i + 1) + ".png");
    state.frame_clock.surface = state.surface;
    state.frame_clock.draw = [&state] { overseer_paint(state); };
    return true;
}

void overseer_request_frame(OverseerState &state) {
    if (state.egl_surface == EGL_NO_SURFACE || !state.open)
        return;
    request_frame(state.frame_clock);
}

void overseer_destroy_surface(OverseerState &state) {
    if (state.frame_clock.callback) {
        wl_callback_destroy(state.frame_clock.callback);
        state.frame_clock.callback = nullptr;
    }
    state.frame_clock.surface = nullptr;
    state.frame_clock.redraw_requested = false;
    state.frame_clock.mapped = false;
    if (state.egl_surface != EGL_NO_SURFACE) {
        eglDestroySurface(state.egl_display, state.egl_surface);
        state.egl_surface = EGL_NO_SURFACE;
    }
    if (state.egl_window) {
        wl_egl_window_destroy(state.egl_window);
        state.egl_window = nullptr;
    }
    if (state.layer_surface) {
        zwlr_layer_surface_v1_destroy(state.layer_surface);
        state.layer_surface = nullptr;
    }
    if (state.surface) {
        wl_surface_destroy(state.surface);
        state.surface = nullptr;
    }
    state.configured = false;
}

void overseer_retarget(OverseerState &state, wl_compositor *compositor,
                       zwlr_layer_shell_v1 *layer_shell, wl_display *display,
                       Renderer &renderer, EGLDisplay egl_display,
                       EGLConfig egl_config, EGLContext egl_context,
                       wl_output *target_output, const char *target_name) {
    wl_output *previous_output = state.bound_output;
    klog("panel: overseer retargeting from output=%p to '%s'",
         static_cast<void *>(previous_output), target_name);

    overseer_destroy_surface(state);
    state.open = false;
    state.opacity = 0.0f;

    auto bind_to = [&](wl_output *out) -> bool {
        if (!overseer_create_surface(state, compositor, layer_shell, out))
            return false;
        while (!state.configured)
            wl_display_dispatch(display);
        return overseer_init_egl(state, renderer, egl_display, egl_config,
                                 egl_context);
    };

    if (bind_to(target_output)) {
        state.bound_output = target_output;
        return;
    }
    if (previous_output && bind_to(previous_output)) {
        state.bound_output = previous_output;
        return;
    }
    klog("panel: overseer retarget fallback also failed");
}

void overseer_search_start_pending(OverseerState &state) {
    if (!state.awaiting_restart)
        return;
    bool still_alive = async_process_is_alive(state.pending_kill_dirs) ||
                       async_process_is_alive(state.pending_kill_files);
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - state.pending_kill_since)
            .count();
    if (still_alive && elapsed < kOverseerKillGraceMs)
        return;
    klog("overseer: pending restart fired (confirmed_dead=%d) after %lldms "
         "wait",
         !still_alive, static_cast<long long>(elapsed));
    state.awaiting_restart = false;
    state.pending_kill_dirs = -1;
    state.pending_kill_files = -1;
    state.effective_query = state.pending_restart_query;
    overseer_search_start_now(state);
}

bool overseer_search_poll(OverseerState &state) {
    if (!state.search_running)
        return false;
    bool dirs_done = async_process_poll(state.search_dirs_proc);
    bool files_done = async_process_poll(state.search_files_proc);
    if (!dirs_done || !files_done)
        return false;

    state.search_running = false;
    std::vector<ScoredApp> apps = search_apps(state.apps, state.search_query);
    std::vector<FileEntry> files;
    for (bool want_dirs : {true, false}) {
        const std::string &raw = want_dirs ? state.search_dirs_proc.buffer
                                           : state.search_files_proc.buffer;
        for (FileEntry &fe : fd_search_parse_output(raw, want_dirs)) {
            fe.score = score_path(fe.name, state.search_query);
            if (fe.score >= 0.0f)
                files.push_back(std::move(fe));
        }
    }
    state.results =
        combined_drun_results(apps, files, state.visits, kOverseerMaxResults);
    state.selected_index = state.results.empty() ? -1 : 0;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - state.search_started_at)
                  .count();
    klog("overseer: search_poll DONE after %lldms query='%s' results=%zu",
         static_cast<long long>(ms), state.search_query.c_str(),
         state.results.size());
    return true;
}

const Texture *overseer_icon_lookup(OverseerState &state, const std::string &id,
                                    const std::string &icon_field) {
    auto it = state.app_icon_cache.find(id);
    if (it == state.app_icon_cache.end()) {
        std::string path = resolve_app_icon_path(icon_field);
        it = state.app_icon_cache
                 .emplace(id, path.empty()
                                  ? Texture{}
                                  : load_image_texture(path, kIconTargetSize))
                 .first;
    }
    return it->second.id ? &it->second : nullptr;
}

int overseer_poll_timeout_ms(const OverseerState &state) {
    int timeout_ms = -1;
    if (state.search_dirty) {
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - state.search_dirty_at)
                .count();
        timeout_ms = static_cast<int>(
            std::max<long long>(0, kOverseerSearchDebounceMs - elapsed));
    }
    if (state.awaiting_restart)
        timeout_ms = timeout_ms < 0
                         ? kOverseerKillCheckMs
                         : std::min(timeout_ms, kOverseerKillCheckMs);
    return timeout_ms;
}

bool overseer_tick(OverseerState &state) {
    if (!state.search_dirty || overseer_poll_timeout_ms(state) > 0)
        return false;
    state.search_dirty = false;
    if (state.mode == OverseerMode::Drun && !state.effective_query.empty()) {
        overseer_search_start(state);
    } else {
        state.results.clear();
        state.selected_index = -1;
    }
    return true;
}

void overseer_toggle(OverseerState &state, bool global) {
    if (!state.layer_surface || state.egl_surface == EGL_NO_SURFACE)
        return;

    if (state.open) {
        klog("overseer: CLOSE (was_search_running=%d dirs_pid=%d "
             "files_pid=%d)",
             state.search_running, async_process_pid(state.search_dirs_proc),
             async_process_pid(state.search_files_proc));
        state.search_dirty = false;
        async_process_cancel(state.search_dirs_proc);
        async_process_cancel(state.search_files_proc);
        state.search_running = false;
        state.awaiting_restart = false;
        state.pending_kill_dirs = state.pending_kill_files = -1;
        state.search.preedit.clear();
        if (state.sync_text_input_focus)
            state.sync_text_input_focus(false);

        state.animations.animate(
            state.opacity, 0.0f, kOverlayFadeMs, Easing::EaseOutCubic,
            [&state](float v) { state.opacity = v; },
            [&state] {
                state.open = false;
                state.search.text.clear();
                overseer_query_char_clear(state);
                state.effective_query.clear();
                state.mode = OverseerMode::Drun;
                state.results.clear();
                state.selected_index = -1;
                state.hovered_index = -1;
                state.anim_height_target = -1.0f;
                state.highlight_offset_target = -1.0f;
                state.scroll_offset_target = -1.0f;
                submenu_close(state.submenu);
                zwlr_layer_surface_v1_set_keyboard_interactivity(
                    state.layer_surface,
                    ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
                overseer_update_input_region(state);
                wl_surface_commit(state.surface);
                DeferredCall::call_later([&state] {
                    if (!state.open)
                        overseer_destroy_surface(state);
                });
            },
            kOverlayFadeOwner);
        overseer_request_frame(state);
        return;
    }

    klog("overseer: OPEN (global=%d)", global);
    const char *home = getenv("HOME");
    state.search_root = global ? "/" : (home ? home : "/");
    state.apps = scan_desktop_entries();
    state.open = true;
    state.search.cursor_blink_visible = true;
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        state.layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
    overseer_update_input_region(state);
    wl_surface_commit(state.surface);
    state.animations.animate(
        state.opacity, 1.0f, kOverlayFadeMs, Easing::EaseOutCubic,
        [&state](float v) { state.opacity = v; }, {}, kOverlayFadeOwner);
    overseer_request_frame(state);
    if (state.sync_text_input_focus)
        state.sync_text_input_focus(true);
}

void overseer_handle_key_event(OverseerState &state, const KeyEvent &event) {
    state.hovered_index = -1;
    switch (event.kind) {
    case KeyKind::Text:

        if (state.submenu.screen != SubmenuScreen::Search)
            submenu_close(state.submenu);
        state.search.text += event.text;
        state.search.preedit.clear();
        overseer_query_changed(state);
        break;

    case KeyKind::Preedit:
        state.search.preedit = event.text;
        state.search.cursor_blink_visible = true;
        break;

    case KeyKind::Backspace: {
        if (state.submenu.screen != SubmenuScreen::Search)
            submenu_close(state.submenu);

        text_field_backspace(state.search.text);
        state.search.preedit.clear();
        overseer_query_changed(state);
        break;
    }

    case KeyKind::Up:
    case KeyKind::Down: {
        int count = state.submenu.screen == SubmenuScreen::Search
                        ? static_cast<int>(state.results.size())
                        : static_cast<int>(state.submenu.items.size());
        if (count == 0) {
            state.selected_index = -1;
            break;
        }
        int delta = event.kind == KeyKind::Down ? 1 : -1;
        state.selected_index =
            std::clamp(state.selected_index + delta, 0, count - 1);
        break;
    }

    case KeyKind::Escape:
        if (state.submenu.screen != SubmenuScreen::Search) {
            submenu_go_back(state.submenu, overseer_dir_lister);
            if (state.submenu.screen == SubmenuScreen::Search)
                state.selected_index = state.results.empty() ? -1 : 0;
            else
                state.selected_index = state.submenu.items.empty() ? -1 : 0;
        } else {
            overseer_toggle(state, false);
        }
        break;

    case KeyKind::Enter:
        overseer_launch_selected(state);
        break;

    case KeyKind::Tab:
    case KeyKind::Left:
    case KeyKind::Right:
        break;
    }
}

namespace {

int overseer_row_at(const OverseerState &state, double px, double py) {
    for (const OverseerRowHit &h : state.row_hitboxes) {
        const Rect &r = h.rect;
        if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h)
            return h.index;
    }
    return -1;
}

} // namespace

void overseer_handle_click(OverseerState &state, double px, double py) {
    int row = overseer_row_at(state, px, py);
    if (row >= 0) {
        state.selected_index = row;
        state.hovered_index = -1;
        overseer_launch_selected(state);
        return;
    }
    const Rect &r = state.box_rect;
    bool inside = px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    if (!inside)
        overseer_toggle(state, false);
}

void overseer_handle_pointer_move(OverseerState &state,
                                  wl_surface *focused_surface, double px,
                                  double py) {
    int prev = state.hovered_index;
    if (!state.open || focused_surface != state.surface)
        state.hovered_index = -1;
    else
        state.hovered_index = overseer_row_at(state, px, py);
    if (state.hovered_index != prev)
        overseer_request_frame(state);
}

namespace {

std::string elide(const std::string &s) {
    if (s.size() <= overseer_detail::kMaxRowChars)
        return s;
    size_t cut = overseer_detail::kMaxRowChars - 1;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80)
        --cut;
    return s.substr(0, cut) + "…";
}

const char *mode_icon(OverseerMode mode) {
    switch (mode) {
    case OverseerMode::Run:
        return icon::terminal;
    case OverseerMode::Google:
        return icon::brand_google;
    case OverseerMode::YouTube:
        return icon::brand_youtube;
    case OverseerMode::DuckDuckGo:
    case OverseerMode::Url:
        return icon::link;
    case OverseerMode::Drun:
    default:
        return icon::apps;
    }
}

struct Row {
    const char *icon;
    std::string label;
    std::string subtitle;
    const Texture *icon_tex = nullptr;
};

std::vector<Row> visible_rows(OverseerState &state, int &first) {
    std::vector<Row> rows;
    if (state.submenu.screen == SubmenuScreen::Search) {
        for (const DrunResult &r : state.results) {
            switch (r.kind) {
            case DrunResult::Kind::App: {
                Row row{icon::apps, r.app->name, ""};
                row.icon_tex =
                    overseer_icon_lookup(state, r.app->id, r.app->icon);
                rows.push_back(std::move(row));
                break;
            }
            case DrunResult::Kind::Dir:
                rows.push_back({icon::folder, r.file.name,
                                path_collapse_home(r.file.path)});
                break;
            case DrunResult::Kind::File:
                rows.push_back(
                    {icon::edit, r.file.name, path_collapse_home(r.file.path)});
                break;
            }
        }
    } else {
        for (const SubmenuEntry &e : state.submenu.items) {
            std::string subtitle =
                e.path.empty() ? "" : path_collapse_home(e.path);
            const char *row_icon =
                e.icon ? e.icon : (e.is_dir ? icon::folder : icon::edit);
            rows.push_back({row_icon, e.name, subtitle});
        }
    }

    first = 0;
    if (state.selected_index >= kOverseerMaxVisible)
        first = state.selected_index - kOverseerMaxVisible + 1;
    return rows;
}

const Texture *cached_text(TextureCache &cache, const std::string &s,
                           int32_t scale) {
    if (s.empty())
        return nullptr;
    return cache.get("t" + std::to_string(scale) + ":" + s,
                     [&] { return rasterize_text(s, scale); });
}

const Texture *cached_text_small(TextureCache &cache, const std::string &s,
                                 int32_t scale) {
    if (s.empty())
        return nullptr;
    return cache.get("s" + std::to_string(scale) + ":" + s,
                     [&] { return rasterize_text_small(s, scale); });
}

const Texture *cached_icon(TextureCache &cache, const char *codepoint,
                           int32_t scale) {
    return cache.get("i" + std::to_string(scale) + ":" + codepoint,
                     [&] { return rasterize_icon(codepoint, scale); });
}

int overseer_surface_height(int visible_rows) {
    float h = kOverseerMenuPad * 2.0f + kOverseerSearchHeight;
    if (visible_rows > 0)
        h += kOverseerListGap + visible_rows * kOverseerRowHeight +
             (visible_rows - 1) * kOverseerRowSpacing;
    return static_cast<int>(h);
}

} // namespace

void overseer_paint(OverseerState &state) {
    if (state.egl_surface == EGL_NO_SURFACE)
        return;

    state.animations.tick(std::chrono::steady_clock::now());
    state.row_hitboxes.clear();

    int first = 0;
    std::vector<Row> rows;
    if (state.open)
        rows = visible_rows(state, first);

    int visible_count =
        static_cast<int>(std::min<size_t>(rows.size(), kOverseerMaxVisible));
    int content_h = overseer_surface_height(visible_count);

    if (state.anim_height_target < 0.0f) {
        state.anim_height = static_cast<float>(content_h);
        state.anim_height_target = static_cast<float>(content_h);
    } else if (static_cast<float>(content_h) != state.anim_height_target) {
        state.anim_height_target = static_cast<float>(content_h);
        state.animations.animate(
            state.anim_height, state.anim_height_target, kOverseerHeightAnimMs,
            Easing::EaseInOutCubic,
            [&state](float v) { state.anim_height = v; }, {},
            kOverseerHeightOwner);
    }

    eglMakeCurrent(state.egl_display, state.egl_surface, state.egl_surface,
                   state.egl_context);
    int32_t scale = state.output_scale.scale;
    state.renderer->begin_frame(state.width, state.height, scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    auto tex_w = [](const Texture *t) {
        return t ? static_cast<float>(t->width) /
                       static_cast<float>(t->scale > 0 ? t->scale : 1)
                 : 0.0f;
    };
    auto tex_h = [](const Texture *t) {
        return t ? static_cast<float>(t->height) /
                       static_cast<float>(t->scale > 0 ? t->scale : 1)
                 : 0.0f;
    };

    state.scene.rebuild();
    Node *root = &state.scene.root;

    if (state.open) {
        const float *white = rgba(palette::text);

        float box_h = state.anim_height;
        float box_x =
            (static_cast<float>(state.width) - kOverseerSurfaceWidth) / 2.0f;
        float box_y = (static_cast<float>(state.height) - box_h) / 2.0f;
        state.box_rect = {box_x, box_y, kOverseerSurfaceWidth, box_h};

        node_add_rrect(root, box_x, box_y, kOverseerSurfaceWidth, box_h,
                       metrics::radius_md, kOverseerMenuBorderWidth,
                       rgba(palette::base_alpha80), rgba(palette::accent));

        float clip_inset = metrics::radius_md;
        Node *outer =
            node_add_group(root, box_x + clip_inset, box_y + clip_inset,
                           kOverseerSurfaceWidth - 2 * clip_inset,
                           box_h - 2 * clip_inset, true);
        auto orx = [&](float v) { return v - (box_x + clip_inset); };
        auto ory = [&](float v) { return v - (box_y + clip_inset); };

        constexpr float kTransparent[4] = {0, 0, 0, 0};
        float mode_box_x = box_x + kOverseerMenuPad;
        float mode_box_w = kOverseerSearchHeight;
        node_add_rrect(outer, orx(mode_box_x), ory(box_y + kOverseerMenuPad),
                       mode_box_w, kOverseerSearchHeight, metrics::radius_sm,
                       kOverseerBorderWidth, kTransparent,
                       rgba(palette::accent));
        const Texture *mode_tex =
            cached_icon(state.tcache, mode_icon(state.mode), scale);
        if (mode_tex) {
            node_add_texture(
                outer, orx(mode_box_x + (mode_box_w - tex_w(mode_tex)) / 2.0f),
                ory(box_y + kOverseerMenuPad +
                    (kOverseerSearchHeight - tex_h(mode_tex)) / 2.0f),
                *mode_tex, white);
        }

        float field_box_x = mode_box_x + mode_box_w + kOverseerPad;
        float field_box_w =
            box_x + kOverseerSurfaceWidth - kOverseerMenuPad - field_box_x;
        node_add_rrect(outer, orx(field_box_x), ory(box_y + kOverseerMenuPad),
                       field_box_w, kOverseerSearchHeight, metrics::radius_sm,
                       kOverseerBorderWidth, kTransparent,
                       rgba(palette::accent));
        float text_x = field_box_x + kOverseerPad;
        float field_center_y =
            box_y + kOverseerMenuPad + kOverseerSearchHeight / 2.0f;

        std::string display = elide(state.search.text);
        float advance = draw_text_field_value(
            outer, state.tcache, scale, display, orx(text_x),
            ory(field_center_y), white, &state.query_anim);
        float cx = text_x + advance;

        draw_text_field_preedit(outer, state.tcache, scale,
                                state.search.preedit, orx(cx),
                                ory(field_center_y), white);

        float caret_h = kOverseerSearchHeight - 2.0f * kOverseerPad;
        state.search.cursor_rect = {cx, field_center_y - caret_h / 2.0f,
                                    kCaretW, caret_h};
        draw_text_field_caret(
            outer, state.search,
            {orx(cx), ory(field_center_y - caret_h / 2.0f), kCaretW, caret_h},
            rgba(palette::text), true);

        float content_x = mode_box_x + mode_box_w + kOverseerBulletGap;
        float list_top = box_y + kOverseerListTop;
        float list_h = box_y + box_h - clip_inset - list_top;
        Node *list_clip = node_add_group(
            outer, orx(mode_box_x), ory(list_top),
            kOverseerSurfaceWidth - 2 * kOverseerMenuPad, list_h, true);

        float row_bg_x = content_x - mode_box_x;
        float row_bg_w =
            box_x + kOverseerSurfaceWidth - kOverseerMenuPad - content_x;

        if (state.selected_index >= 0) {
            float highlight_target =
                static_cast<float>(state.selected_index) * kRowPitch;
            if (state.highlight_offset_target < 0.0f) {
                state.highlight_offset = highlight_target;
                state.highlight_offset_target = highlight_target;
            } else if (highlight_target != state.highlight_offset_target) {
                state.highlight_offset_target = highlight_target;
                state.animations.animate(
                    state.highlight_offset, state.highlight_offset_target,
                    kOverseerHighlightAnimMs, Easing::EaseOutCubic,
                    [&state](float v) { state.highlight_offset = v; }, {},
                    kOverseerHighlightOwner);
            }

            float scroll_target = static_cast<float>(first) * kRowPitch;
            if (state.scroll_offset_target < 0.0f) {
                state.scroll_offset = scroll_target;
                state.scroll_offset_target = scroll_target;
            } else if (scroll_target != state.scroll_offset_target) {
                state.scroll_offset_target = scroll_target;
                state.animations.animate(
                    state.scroll_offset, state.scroll_offset_target,
                    kOverseerHighlightAnimMs, Easing::EaseOutCubic,
                    [&state](float v) { state.scroll_offset = v; }, {},
                    kOverseerScrollOwner);
            }
        } else {
            state.highlight_offset_target = -1.0f;
            state.scroll_offset_target = -1.0f;
        }

        for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
            float y = static_cast<float>(i) * kRowPitch - state.scroll_offset;

            if (y >= 0.0f && y + kOverseerRowHeight <= list_h)
                state.row_hitboxes.push_back(
                    {{content_x, list_top + y, row_bg_w, kOverseerRowHeight},
                     i});

            Node *rowg = node_add_group(
                list_clip, 0, y, kOverseerSurfaceWidth - 2 * kOverseerMenuPad,
                kOverseerRowHeight, true);
            auto lrx = [&](float v) { return v - mode_box_x; };
            auto lry = [&](float v) { return v - y; };

            constexpr float kRowTransparent[4] = {0, 0, 0, 0};
            node_add_rrect(rowg, row_bg_x, 0, row_bg_w, kOverseerRowHeight,
                           metrics::radius_sm, 0.0f,
                           rgba(palette::text_alpha03), kRowTransparent);

            if (i == state.hovered_index && i != state.selected_index)
                node_add_rrect(rowg, row_bg_x, 0, row_bg_w, kOverseerRowHeight,
                               metrics::radius_sm, kOverseerBorderWidth,
                               kRowTransparent, rgba(palette::accent));

            float rowx = content_x + kOverseerPad;
            if (rows[i].icon_tex) {
                const Texture &tex = *rows[i].icon_tex;
                node_add_texture_rect(
                    rowg, lrx(rowx),
                    lry(y + (kOverseerRowHeight - kIconTargetSize) / 2.0f),
                    kIconTargetSize, kIconTargetSize, tex, white);
            } else {
                const Texture *row_icon =
                    cached_icon(state.tcache, rows[i].icon, scale);
                if (row_icon) {
                    node_add_texture(
                        rowg,
                        lrx(rowx + (kIconTargetSize - tex_w(row_icon)) / 2.0f),
                        lry(y + (kOverseerRowHeight - tex_h(row_icon)) / 2.0f),
                        *row_icon, white);
                }
            }
            rowx += kIconTargetSize + kOverseerPad;
            const Texture *label =
                cached_text(state.tcache, elide(rows[i].label), scale);
            if (!rows[i].subtitle.empty()) {
                const Texture *subtitle = cached_text_small(
                    state.tcache,
                    elide_middle(rows[i].subtitle,
                                 overseer_detail::kMaxRowChars),
                    scale);
                float th = tex_h(label);
                float sh = tex_h(subtitle);
                float top =
                    y +
                    (kOverseerRowHeight - th - kOverseerTwoLineGap - sh) / 2.0f;
                if (label)
                    node_add_texture(rowg, lrx(rowx), lry(top), *label, white);
                if (subtitle)
                    node_add_texture(rowg, lrx(rowx),
                                     lry(top + th + kOverseerTwoLineGap),
                                     *subtitle, rgba(palette::text_alpha65));
            } else if (label) {
                node_add_texture(
                    rowg, lrx(rowx),
                    lry(y + (kOverseerRowHeight - tex_h(label)) / 2.0f), *label,
                    white);
            }
        }

        for (int slot = 0; slot < kOverseerMaxVisible; ++slot) {
            const Texture &bullet = state.bullet_tex[slot];
            if (!bullet.id)
                continue;
            float slot_y = static_cast<float>(slot) * kRowPitch;
            node_add_texture_rect(
                list_clip, (mode_box_w - kOverseerBulletSize) / 2.0f,
                slot_y + (kOverseerRowHeight - kOverseerBulletSize) / 2.0f,
                kOverseerBulletSize, kOverseerBulletSize, bullet, white);
        }

        if (state.selected_index >= 0) {
            constexpr float kTransparent2[4] = {0, 0, 0, 0};
            node_add_rrect(list_clip, content_x - mode_box_x,
                           state.highlight_offset - state.scroll_offset,
                           box_x + kOverseerSurfaceWidth - kOverseerMenuPad -
                               content_x,
                           kOverseerRowHeight, metrics::radius_sm,
                           kOverseerHighlightBorderWidth, kTransparent2,
                           rgba(palette::accent_alt_alpha50));
        }
    }

    state.renderer->set_opacity(state.opacity);
    state.scene.draw(*state.renderer);
    state.renderer->set_opacity(1.0f);
    eglSwapBuffers(state.egl_display, state.egl_surface);

    if (state.animations.hasActive())
        overseer_request_frame(state);
}

TextInputState overseer_text_input_state(const OverseerState &state) {
    TextInputState s;
    s.purpose = TextInputPurpose::Normal;
    s.cursor_rect_x = static_cast<int32_t>(state.search.cursor_rect.x);
    s.cursor_rect_y = static_cast<int32_t>(state.search.cursor_rect.y);
    s.cursor_rect_w = static_cast<int32_t>(state.search.cursor_rect.w);
    s.cursor_rect_h = static_cast<int32_t>(state.search.cursor_rect.h);
    return s;
}

void overseer_text_input_apply_edit(OverseerState &state,
                                    const TextInputEdit &edit) {
    if (edit.has_delete)
        for (uint32_t i = 0; i < edit.delete_before_length; ++i)
            text_field_backspace(state.search.text);
    if (edit.has_commit_text) {
        state.search.text += edit.commit_text;
        state.search.preedit.clear();
    }
    if (edit.has_delete || edit.has_commit_text)
        overseer_query_changed(state);
    if (edit.has_preedit)
        state.search.preedit = edit.preedit_text;
    state.search.cursor_blink_visible = true;
}
