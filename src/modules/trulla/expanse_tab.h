#pragma once

#include <EGL/egl.h>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "render/texture.h"

struct Config;
struct Node;
struct PanelClickRegion;
struct TrullaState;

inline constexpr size_t kExpansePickerMaxInFlight = 2;

struct ExpansePickerState {
    std::string dir;
    bool scanning = false;
    std::vector<std::string> files;
    std::map<std::string, Texture> thumbnails;
    std::set<std::string> pending;
    uint64_t scan_generation = 0;

    std::function<void()> request_frame;
};

struct ExpanseSubtabState {
    ExpansePickerState picker;
    std::string selected_region;
    int selected_column = 0;
    float scroll_offset = 0.0f;
    float grid_width = 0.0f;
    float grid_height = 0.0f;
};

bool expanse_picker_less(const std::string &a, const std::string &b);

bool expanse_picker_is_image(const std::string &path);

bool expanse_picker_is_video(const std::string &path);

void expanse_picker_scan(ExpansePickerState &state, std::string dir,
                         bool (*is_match)(const std::string &));

void expanse_picker_request_thumbnail(ExpansePickerState &state,
                                      const std::string &path, int target_size,
                                      EGLDisplay display, EGLSurface surface,
                                      EGLContext context);

float expanse_tab_paint(TrullaState &state, Node *root, int32_t scale, float x,
                        float y, const Config &cfg);

bool expanse_tab_handle_click(TrullaState &state, const Config &cfg,
                              const std::function<void(Config)> &on_commit,
                              const PanelClickRegion &region);

void expanse_tab_handle_scroll(TrullaState &state, double dy);
