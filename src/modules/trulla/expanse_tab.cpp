#include <algorithm>
#include <filesystem>
#include <thread>

#include "core/deferred_call.h"
#include "core/path_home.h"

#include "modules/trulla.h"
#include "modules/trulla/expanse_tab.h"

#include "render/panel_scroll.h"

#include "service/expanse_service.h"
#include "service/media_service.h"

using panel_chrome_detail::cached_icon;
using panel_chrome_detail::cached_text;
using panel_chrome_detail::cached_text_clipped;

bool expanse_picker_is_image(const std::string &path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

bool expanse_picker_is_video(const std::string &path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return ext == ".mp4" || ext == ".webm" || ext == ".mkv";
}

namespace {

std::string expanse_picker_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // namespace

bool expanse_picker_less(const std::string &a, const std::string &b) {
    std::filesystem::path pa(a), pb(b);
    std::string ea = expanse_picker_lower(pa.extension().string());
    std::string eb = expanse_picker_lower(pb.extension().string());
    if (ea != eb)
        return ea < eb;
    return expanse_picker_lower(pa.filename().string()) <
           expanse_picker_lower(pb.filename().string());
}

void expanse_picker_scan(ExpansePickerState &state, std::string dir,
                         bool (*is_match)(const std::string &)) {
    state.dir = dir;
    state.scanning = true;
    uint64_t generation = ++state.scan_generation;
    std::thread([&state, dir, generation, is_match] {
        std::vector<std::string> found;
        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(
            dir, std::filesystem::directory_options::skip_permission_denied,
            ec);
        std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            if (it.depth() >= 1)
                it.disable_recursion_pending();
            if (it->is_regular_file(ec) && is_match(it->path().string()))
                found.push_back(it->path().string());
        }
        std::sort(found.begin(), found.end(), expanse_picker_less);
        DeferredCall::call_later(
            [&state, found = std::move(found), generation] {
                if (generation != state.scan_generation)
                    return;
                state.files = std::move(found);
                state.scanning = false;
                if (state.request_frame)
                    state.request_frame();
            });
    }).detach();
}

void expanse_picker_request_thumbnail(ExpansePickerState &state,
                                      const std::string &path, int target_size,
                                      EGLDisplay display, EGLSurface surface,
                                      EGLContext context) {
    if (state.thumbnails.count(path) || state.pending.count(path))
        return;
    if (state.pending.size() >= kExpansePickerMaxInFlight)
        return;
    state.pending.insert(path);
    uint64_t generation = state.scan_generation;
    std::thread([&state, path, target_size, generation, display, surface,
                 context] {
        int w = 0, h = 0;
        unsigned char *data =
            animate_decode_scaled(path, target_size, target_size, w, h);
        DeferredCall::call_later(
            [&state, path, data, w, h, generation, display, surface, context] {
                state.pending.erase(path);
                if (generation != state.scan_generation) {
                    delete[] data;
                    return;
                }
                if (!data)
                    return;
                eglMakeCurrent(display, surface, surface, context);
                state.thumbnails[path] = make_texture_rgba(w, h, data, true);
                delete[] data;
                if (state.request_frame)
                    state.request_frame();
            });
    }).detach();
}

namespace {

int expanse_grid_columns(float) { return kTrullaExpanseGridColumns; }

float expanse_grid_content_height(const ExpansePickerState &picker,
                                  float content_w) {
    int cols = expanse_grid_columns(content_w);
    size_t rows = (picker.files.size() + cols - 1) / static_cast<size_t>(cols);
    if (rows == 0)
        return 0.0f;
    float cell = kTrullaExpanseThumbSize + kTrullaExpanseThumbGap;
    return static_cast<float>(rows) * cell - kTrullaExpanseThumbGap;
}

int expanse_column_count(const Config &cfg, const std::string &region,
                         bool animated) {
    return expanse_service_column_count(cfg, region, animated);
}

std::string expanse_column_path(const Config &cfg, const std::string &region,
                                int column, bool animated) {
    return expanse_service_column_path(cfg, region, column, animated);
}

std::string expanse_column_override(const Config &cfg,
                                    const std::string &region, int column,
                                    bool animated) {
    return expanse_service_column_override(cfg, region, column, animated);
}

std::string expanse_fill_mode(const Config &cfg, const std::string &region,
                              int column, bool animated) {
    return expanse_service_fill_mode(cfg, region, column, animated);
}

void set_expanse_column(Config &cfg, const std::string &monitor, int column,
                        const std::string &path, bool animated) {
    auto &all_cols =
        animated ? cfg.expanse_animated_columns : cfg.expanse_columns;
    std::vector<std::string> &cols = all_cols[monitor];
    if (static_cast<size_t>(column) >= cols.size())
        cols.resize(static_cast<size_t>(column) + 1);
    cols[static_cast<size_t>(column)] = path;
}

void set_expanse_fill_mode(Config &cfg, const std::string &monitor, int column,
                           const std::string &mode, bool animated) {
    auto &all_modes =
        animated ? cfg.expanse_animated_fill_modes : cfg.expanse_fill_modes;
    std::vector<std::string> &modes = all_modes[monitor];
    if (static_cast<size_t>(column) >= modes.size())
        modes.resize(static_cast<size_t>(column) + 1);
    modes[static_cast<size_t>(column)] = mode;
}

void draw_expanse_dirbar(TrullaState &state, Node *parent, int32_t scale,
                         float x, float y, float w, ExpanseSubtabState &sub,
                         const std::string &dir, bool animated) {
    TrullaFieldId field_id = animated ? TrullaFieldId::ExpanseAnimatedDir
                                      : TrullaFieldId::ExpanseDir;
    bool focused = state.focused_field == field_id;
    node_add_rrect(parent, x, y, w, kTrullaDirBarHeight, metrics::radius_sm,
                   metrics::border_thin, rgba(palette::field_bg),
                   focused ? rgba(palette::accent) : kPanelNoBorder);

    const Texture *label_tex = cached_text(state.tcache, "Dir", scale);
    float input_x = x + kTrullaDirBarLabelMargin;
    if (label_tex) {
        node_add_texture(parent, input_x,
                         y + (kTrullaDirBarHeight - label_tex->height) / 2.0f,
                         *label_tex, rgba(palette::accent));
        input_x += label_tex->width;
    }
    input_x += kTrullaDirBarFieldMargin;

    float btn_x = x + w - kTrullaDirBarEdgeMargin - kTrullaDirBarButtonWidth;
    float input_w = btn_x - kTrullaDirBarEdgeMargin - input_x;

    float field_center_y = y + kTrullaDirBarHeight / 2.0f;
    if (focused) {
        float advance = draw_text_field_value(
            parent, state.tcache, scale, state.field_buffer.text, input_x,
            field_center_y, rgba(palette::text), &state.field_anim);
        float cursor_x = input_x + advance + 2;
        draw_text_field_preedit(parent, state.tcache, scale,
                                state.field_buffer.preedit, cursor_x,
                                field_center_y, rgba(palette::text));
        Rect caret = {cursor_x, y + 8, 1.5f, kTrullaDirBarHeight - 16};
        state.field_buffer.cursor_rect = caret;
        draw_text_field_caret(parent, state.field_buffer, caret,
                              rgba(palette::text), true);
    } else {
        const Texture *value_tex =
            cached_text_clipped(state.tcache, path_collapse_home(dir), scale,
                                static_cast<int>(input_w));
        if (value_tex)
            node_add_texture(parent, input_x,
                             y + (kTrullaDirBarHeight - value_tex->height) /
                                     2.0f,
                             *value_tex, rgba(palette::text));
    }
    state.click_regions.push_back({PanelClickKind::FieldFocus,
                                   {input_x, y, input_w, kTrullaDirBarHeight},
                                   std::to_string(static_cast<int>(field_id))});

    float btn_y = y + (kTrullaDirBarHeight - kTrullaDirBarButtonHeight) / 2.0f;
    node_add_rrect(parent, btn_x, btn_y, kTrullaDirBarButtonWidth,
                   kTrullaDirBarButtonHeight, metrics::radius_sm, 0.0f,
                   rgba(palette::text_alpha11), kPanelNoBorder);
    const Texture *btn_tex = cached_text(
        state.tcache, sub.picker.scanning ? "\xE2\x80\xA6" : "Rescan", scale);
    if (btn_tex)
        node_add_texture(
            parent, btn_x + (kTrullaDirBarButtonWidth - btn_tex->width) / 2.0f,
            btn_y + (kTrullaDirBarButtonHeight - btn_tex->height) / 2.0f,
            *btn_tex, rgba(palette::text));
    state.click_regions.push_back(
        {PanelClickKind::ToggleFlip,
         {btn_x, btn_y, kTrullaDirBarButtonWidth, kTrullaDirBarButtonHeight},
         animated ? "animatedwallpaperrescan" : "wallpaperrescan"});
}

void draw_region_row(TrullaState &state, Node *parent, int32_t scale, float x,
                     float y, float w, const Config &cfg,
                     ExpanseSubtabState &sub, bool animated) {
    struct Chip {
        std::string name;
        int col;
    };
    std::vector<Chip> chips;
    for (const std::string &name : state.monitor_names) {
        int count = expanse_column_count(cfg, name, animated);
        for (int col = 0; col < count; ++col)
            chips.push_back({name, col});
    }
    if (chips.empty())
        return;

    float gap = kTrullaMonitorChipGap;
    float chip_w =
        (w - (chips.size() - 1) * gap) / static_cast<float>(chips.size());
    float cx = x;
    for (const Chip &chip : chips) {
        int count = expanse_column_count(cfg, chip.name, animated);
        std::string label = count > 1
                                ? chip.name + "-" + std::to_string(chip.col + 1)
                                : chip.name;
        const Texture *tex = cached_text(state.tcache, label, scale);
        bool active =
            chip.name == sub.selected_region && chip.col == sub.selected_column;
        node_add_rrect(parent, cx, y, chip_w, kTrullaMonitorChipHeight,
                       metrics::radius_sm, metrics::border_thin,
                       active ? rgba(palette::accent_alpha19)
                              : rgba(palette::field_bg),
                       active ? rgba(palette::accent_alt) : kPanelNoBorder);
        if (tex)
            node_add_texture(parent, cx + (chip_w - tex->width) / 2.0f,
                             y + (kTrullaMonitorChipHeight - tex->height) /
                                     2.0f,
                             *tex, rgba(palette::text));
        state.click_regions.push_back(
            {animated ? PanelClickKind::AnimatedRegionSelect
                      : PanelClickKind::RegionSelect,
             {cx, y, chip_w, kTrullaMonitorChipHeight},
             chip.name + "|" + std::to_string(chip.col)});
        cx += chip_w + gap;
    }
}

void draw_control_row(TrullaState &state, Node *parent, int32_t scale, float x,
                      float y, float w, const Config &cfg,
                      ExpanseSubtabState &sub, bool animated) {
    int count = sub.selected_region.empty()
                    ? 1
                    : expanse_column_count(cfg, sub.selected_region, animated);
    float step_y = y;
    float sub_x = x;
    node_add_rrect(parent, sub_x, step_y, kTrullaColumnStepperButtonSize,
                   kTrullaFieldHeight, metrics::radius_sm, metrics::border_thin,
                   rgba(palette::field_bg), kPanelNoBorder);
    const Texture *sub_tex = cached_text(state.tcache, "-", scale);
    if (sub_tex)
        node_add_texture(
            parent,
            sub_x + (kTrullaColumnStepperButtonSize - sub_tex->width) / 2.0f,
            step_y + (kTrullaFieldHeight - sub_tex->height) / 2.0f, *sub_tex,
            rgba(palette::text));
    state.click_regions.push_back(
        {PanelClickKind::ToggleFlip,
         {sub_x, step_y, kTrullaColumnStepperButtonSize, kTrullaFieldHeight},
         animated ? "animatedcolumnsub" : "columnsub"});

    const Texture *count_tex =
        cached_text(state.tcache, std::to_string(count), scale);
    float count_w = (count_tex ? count_tex->width : 0) + 12.0f;
    float count_x = sub_x + kTrullaColumnStepperButtonSize;
    if (count_tex)
        node_add_texture(parent, count_x + (count_w - count_tex->width) / 2.0f,
                         step_y +
                             (kTrullaFieldHeight - count_tex->height) / 2.0f,
                         *count_tex, rgba(palette::text));

    float add_x = count_x + count_w;
    node_add_rrect(parent, add_x, step_y, kTrullaColumnStepperButtonSize,
                   kTrullaFieldHeight, metrics::radius_sm, metrics::border_thin,
                   rgba(palette::field_bg), kPanelNoBorder);
    const Texture *add_tex = cached_text(state.tcache, "+", scale);
    if (add_tex)
        node_add_texture(
            parent,
            add_x + (kTrullaColumnStepperButtonSize - add_tex->width) / 2.0f,
            step_y + (kTrullaFieldHeight - add_tex->height) / 2.0f, *add_tex,
            rgba(palette::text));
    state.click_regions.push_back(
        {PanelClickKind::ToggleFlip,
         {add_x, step_y, kTrullaColumnStepperButtonSize, kTrullaFieldHeight},
         animated ? "animatedcolumnadd" : "columnadd"});

    if (!expanse_column_override(cfg, sub.selected_region, sub.selected_column,
                                 animated)
             .empty()) {
        const Texture *tex = cached_text(state.tcache, "Remove", scale);
        float rw = (tex ? tex->width : 0) + 20.0f;
        float rx = x + (w - rw) / 2.0f;
        node_add_rrect(parent, rx, y, rw, kTrullaFieldHeight,
                       metrics::radius_sm, metrics::border_thin,
                       rgba(palette::field_bg), rgba(palette::critical));
        if (tex)
            node_add_texture(parent, rx + 10.0f,
                             y + (kTrullaFieldHeight - tex->height) / 2.0f,
                             *tex, rgba(palette::critical));
        state.click_regions.push_back(
            {PanelClickKind::ToggleFlip,
             {rx, y, rw, kTrullaFieldHeight},
             animated ? "animatedwallpaperremove" : "wallpaperremove"});
    }

    std::string mode = expanse_fill_mode(cfg, sub.selected_region,
                                         sub.selected_column, animated);
    static const char *kLabels[2] = {"Crop", "Fit"};
    static const char *kModes[2] = {"crop", "fit"};
    float widths[2];
    float pair_w = 0.0f;
    for (int i = 0; i < 2; ++i) {
        const Texture *tex = cached_text(state.tcache, kLabels[i], scale);
        widths[i] = (tex ? tex->width : 0) + 20.0f;
        pair_w += widths[i] + (i == 0 ? 0.0f : 6.0f);
    }

    float cx = x + w - pair_w;
    for (int i = 0; i < 2; ++i) {
        bool active = mode == kModes[i];
        const Texture *tex = cached_text(state.tcache, kLabels[i], scale);
        float bw = widths[i];
        node_add_rrect(parent, cx, y, bw, kTrullaFieldHeight,
                       metrics::radius_sm, metrics::border_thin,
                       active ? rgba(palette::accent_alpha19)
                              : rgba(palette::field_bg),
                       active ? rgba(palette::accent) : kPanelNoBorder);
        if (tex)
            node_add_texture(
                parent, cx + 10.0f,
                y + (kTrullaFieldHeight - tex->height) / 2.0f, *tex,
                active ? rgba(palette::accent) : rgba(palette::text));
        state.click_regions.push_back(
            {PanelClickKind::ToggleFlip,
             {cx, y, bw, kTrullaFieldHeight},
             std::string(animated ? "animatedfillmode|" : "fillmode|") +
                 kModes[i]});
        cx += bw + 6.0f;
    }
}

void draw_expanse_grid(TrullaState &state, Node *parent, int32_t scale, float x,
                       float y, float w, float h, const Config &cfg,
                       ExpanseSubtabState &sub, bool animated) {
    ExpansePickerState &picker = sub.picker;
    if (picker.files.empty()) {
        const Texture *t =
            cached_text(state.tcache,
                        picker.scanning ? "Scanning\xE2\x80\xA6"
                                        : (animated ? "No videos found"
                                                    : "No images found"),
                        scale);
        if (t)
            node_add_texture(parent, x, y, *t, rgba(palette::text_dim));
        return;
    }

    node_add_rrect(parent, x, y, w, h, metrics::radius_sm, metrics::border_thin,
                   kPanelNoBorder, rgba(palette::accent));

    float inset_x = x + kTrullaExpanseGridInset;
    float inset_y = y + kTrullaExpanseGridInset;
    float inset_w = w - kTrullaExpanseGridInset * 2.0f;
    float inset_h = h - kTrullaExpanseGridInset * 2.0f;

    int cols = expanse_grid_columns(inset_w);
    float content_h = expanse_grid_content_height(picker, inset_w);
    float visible_h = std::min(inset_h, content_h);
    float cell = kTrullaExpanseThumbSize + kTrullaExpanseThumbGap;
    float row_w = cols * cell - kTrullaExpanseThumbGap;
    inset_x += (inset_w - row_w) / 2.0f;
    std::string selected = expanse_column_path(cfg, sub.selected_region,
                                               sub.selected_column, animated);

    Node *clip =
        node_add_group(parent, inset_x, inset_y, row_w, visible_h, true);
    int total_rows =
        static_cast<int>((picker.files.size() + static_cast<size_t>(cols) - 1) /
                         static_cast<size_t>(cols));
    int first_row = std::clamp(static_cast<int>(sub.scroll_offset / cell), 0,
                               std::max(0, total_rows - 1));
    int last_row =
        std::clamp(static_cast<int>((sub.scroll_offset + visible_h) / cell), 0,
                   std::max(0, total_rows - 1));

    for (int row = first_row; row <= last_row; ++row) {
        for (int col = 0; col < cols; ++col) {
            size_t idx =
                static_cast<size_t>(row) * static_cast<size_t>(cols) + col;
            if (idx >= picker.files.size())
                break;
            const std::string &path = picker.files[idx];
            float cx = col * cell;
            float cy = row * cell - sub.scroll_offset;
            bool active = path == selected;

            auto it = picker.thumbnails.find(path);
            if (it != picker.thumbnails.end()) {
                node_add_rect(clip, cx, cy, kTrullaExpanseThumbSize,
                              kTrullaExpanseThumbSize, rgba(palette::field_bg));

                const Texture &tex = it->second;
                Node *cell_clip =
                    node_add_group(clip, cx, cy, kTrullaExpanseThumbSize,
                                   kTrullaExpanseThumbSize, true);
                float tscale = std::max(
                    kTrullaExpanseThumbSize / static_cast<float>(tex.width),
                    kTrullaExpanseThumbSize / static_cast<float>(tex.height));
                float draw_w = tex.width * tscale;
                float draw_h = tex.height * tscale;
                node_add_texture_rect(cell_clip,
                                      (kTrullaExpanseThumbSize - draw_w) / 2.0f,
                                      (kTrullaExpanseThumbSize - draw_h) / 2.0f,
                                      draw_w, draw_h, tex, rgba(palette::text));
            } else {
                node_add_rrect(clip, cx, cy, kTrullaExpanseThumbSize,
                               kTrullaExpanseThumbSize,
                               kTrullaExpanseThumbRadius, 0.0f,
                               rgba(palette::field_bg), kPanelNoBorder);
                expanse_picker_request_thumbnail(
                    picker, path,
                    static_cast<int>(kTrullaExpanseThumbSize * scale),
                    state.base.egl_display, state.base.egl_surface,
                    state.base.egl_context);
            }

            std::string filename =
                std::filesystem::path(path).filename().string();
            const Texture *name_tex =
                cached_text_clipped(state.tcache, filename, scale,
                                    static_cast<int>(kTrullaExpanseThumbSize -
                                                     kTrullaExpanseLabelPad));
            float label_h =
                (name_tex ? name_tex->height : 0) + kTrullaExpanseLabelPad;
            node_add_rect(clip, cx, cy + kTrullaExpanseThumbSize - label_h,
                          kTrullaExpanseThumbSize, label_h,
                          rgba(palette::overlay));
            if (name_tex)
                node_add_texture(clip, cx + kTrullaExpanseLabelPad / 2.0f,
                                 cy + kTrullaExpanseThumbSize -
                                     (label_h + name_tex->height) / 2.0f,
                                 *name_tex, rgba(palette::text));

            node_add_rrect(clip, cx, cy, kTrullaExpanseThumbSize,
                           kTrullaExpanseThumbSize, kTrullaExpanseThumbRadius,
                           active ? metrics::border_thick : 0.0f,
                           kPanelNoBorder,
                           active ? rgba(palette::accent_alt) : kPanelNoBorder);
            state.click_regions.push_back(
                {animated ? PanelClickKind::AnimatedExpanseSelect
                          : PanelClickKind::ExpanseSelect,
                 {inset_x + cx, inset_y + cy, kTrullaExpanseThumbSize,
                  kTrullaExpanseThumbSize},
                 path});
        }
    }
}

float draw_expanse_decode_warning(TrullaState &state, Node *parent,
                                  int32_t scale, float x, float y, float w) {
    const Texture *tex = cached_text(
        state.tcache, "Warning: Animated wallpaper is running on CPU.", scale);
    float text_h = tex ? tex->height : 0.0f;
    float row_h = text_h + kTrullaExpanseWarningPad * 2.0f;
    node_add_rrect(parent, x, y, w, row_h, metrics::radius_sm,
                   metrics::border_thin, rgba(palette::critical_alpha15),
                   rgba(palette::critical));
    if (tex)
        node_add_texture(parent, x + kTrullaExpanseWarningPad,
                         y + (row_h - text_h) / 2.0f, *tex,
                         rgba(palette::critical));
    return row_h;
}

} // namespace

float expanse_tab_paint(TrullaState &state, Node *root, int32_t scale, float x,
                        float y, const Config &cfg) {
    bool animated = cfg.expanse_animated_enabled;
    state.expanse_animated_active = animated;
    ExpanseSubtabState &sub =
        animated ? state.expanse_animated : state.expanse_static;
    std::string dir = animated ? cfg.expanse_animated_dir : cfg.expanse_dir;
    if (sub.picker.dir != dir)
        expanse_picker_scan(sub.picker, dir,
                            animated ? expanse_picker_is_video
                                     : expanse_picker_is_image);

    sub.grid_width =
        state.panel_rect.x + state.panel_rect.w - kPanelPadding - x;
    draw_toggle_row(state, root, scale, x, y, sub.grid_width,
                    "Use Default Wallpaper", cfg.default_expanse_enabled,
                    "usedefaultwallpaper", true);
    y += kTrullaToggleTileHeight + kPanelRowGap;
    draw_toggle_row(state, root, scale, x, y, sub.grid_width,
                    "Enable animated wallpaper", cfg.expanse_animated_enabled,
                    "enableanimatedwallpaper", true);
    y += kTrullaToggleTileHeight + kPanelRowGap;
    draw_region_row(state, root, scale, x, y, sub.grid_width, cfg, sub,
                    animated);
    y += kTrullaMonitorChipHeight + kPanelRowGap;
    draw_expanse_dirbar(state, root, scale, x, y, sub.grid_width, sub, dir,
                        animated);
    y += kTrullaDirBarHeight + kPanelRowGap;
    draw_control_row(state, root, scale, x, y, sub.grid_width, cfg, sub,
                     animated);
    y += kTrullaRowHeight;
    float grid_available_h =
        state.panel_rect.y + state.panel_rect.h - kPanelPadding - y;
    float grid_inset_w = sub.grid_width - kTrullaExpanseGridInset * 2.0f;
    float grid_content_h =
        expanse_grid_content_height(sub.picker, grid_inset_w);
    sub.grid_height = std::min(grid_available_h,
                               grid_content_h + kTrullaExpanseGridInset * 2.0f);
    draw_expanse_grid(state, root, scale, x, y, sub.grid_width, sub.grid_height,
                      cfg, sub, animated);
    y += sub.grid_height;
    if (animated && state.expanse_decode_status &&
        state.expanse_decode_status(sub.selected_region, sub.selected_column) ==
            MediaDecodeStatus::CpuFallback) {
        y += kPanelRowGap;
        y += draw_expanse_decode_warning(state, root, scale, x, y,
                                         sub.grid_width);
    }
    return y;
}

bool expanse_tab_handle_click(TrullaState &state, const Config &cfg,
                              const TrullaCommitFn &on_commit,
                              const PanelClickRegion &region) {
    if (region.kind == PanelClickKind::ExpanseSelect ||
        region.kind == PanelClickKind::AnimatedExpanseSelect) {
        bool animated = region.kind == PanelClickKind::AnimatedExpanseSelect;
        ExpanseSubtabState &sub =
            animated ? state.expanse_animated : state.expanse_static;
        trulla_commit_focused_field(state, cfg, on_commit);
        Config updated = cfg;
        set_expanse_column(updated, sub.selected_region, sub.selected_column,
                           region.tag, animated);
        on_commit(updated);
        trulla_request_frame(state);
        return true;
    }
    if (region.kind == PanelClickKind::RegionSelect ||
        region.kind == PanelClickKind::AnimatedRegionSelect) {
        bool animated = region.kind == PanelClickKind::AnimatedRegionSelect;
        ExpanseSubtabState &sub =
            animated ? state.expanse_animated : state.expanse_static;
        size_t sep = region.tag.find('|');
        sub.selected_region = region.tag.substr(0, sep);
        sub.selected_column = sep == std::string::npos
                                  ? 0
                                  : std::stoi(region.tag.substr(sep + 1));
        trulla_request_frame(state);
        return true;
    }
    if (region.kind != PanelClickKind::ToggleFlip)
        return false;

    trulla_commit_focused_field(state, cfg, on_commit);
    if (region.tag == "enableanimatedwallpaper") {
        Config updated = cfg;
        updated.expanse_animated_enabled = !cfg.expanse_animated_enabled;
        on_commit(updated);
        trulla_request_frame(state);
        return true;
    }
    if (region.tag == "usedefaultwallpaper") {
        Config updated = cfg;
        updated.default_expanse_enabled = !cfg.default_expanse_enabled;
        on_commit(updated);
        trulla_request_frame(state);
        return true;
    }

    bool animated = region.tag.rfind("animated", 0) == 0;
    ExpanseSubtabState &sub =
        animated ? state.expanse_animated : state.expanse_static;
    std::string tag = animated ? region.tag.substr(8) : region.tag;

    if (tag.rfind("fillmode|", 0) == 0) {
        Config updated = cfg;
        set_expanse_fill_mode(updated, sub.selected_region, sub.selected_column,
                              tag.substr(9), animated);
        on_commit(updated);
    } else if (tag == "wallpaperremove") {
        Config updated = cfg;
        set_expanse_column(updated, sub.selected_region, sub.selected_column,
                           "", animated);
        on_commit(updated);
    } else if (tag == "wallpaperrescan") {
        expanse_picker_scan(
            sub.picker, animated ? cfg.expanse_animated_dir : cfg.expanse_dir,
            animated ? expanse_picker_is_video : expanse_picker_is_image);
    } else if (tag == "columnadd" || tag == "columnsub") {
        Config updated = cfg;
        int count = expanse_column_count(cfg, sub.selected_region, animated);
        count = std::clamp(count + (tag == "columnadd" ? 1 : -1), 1, 6);
        auto &counts_map = animated ? updated.expanse_animated_column_counts
                                    : updated.expanse_column_counts;
        counts_map[sub.selected_region] = count;
        on_commit(updated);
    } else {
        return false;
    }
    trulla_request_frame(state);
    return true;
}

void expanse_tab_handle_scroll(TrullaState &state, double dy) {
    ExpanseSubtabState &sub = state.expanse_animated_active
                                  ? state.expanse_animated
                                  : state.expanse_static;
    float inset_w = sub.grid_width - kTrullaExpanseGridInset * 2.0f;
    float inset_h = sub.grid_height - kTrullaExpanseGridInset * 2.0f;
    float content_h = expanse_grid_content_height(sub.picker, inset_w);
    sub.scroll_offset = panel_clamp_scroll(
        sub.scroll_offset, static_cast<float>(dy) * kTrullaExpanseScrollSpeed,
        content_h, inset_h);
    trulla_request_frame(state);
}
