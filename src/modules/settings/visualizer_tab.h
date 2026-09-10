#pragma once

#include <string>

#include "modules/settings.h"

std::string visualizer_field_text(const VisualizerParams &params,
                                 SettingsFieldId id);

void visualizer_tab_paint(SettingsState &state, Node *root, int32_t scale, float x,
                         float y, float w, const Config &cfg);

bool visualizer_tab_handle_click(SettingsState &state, const Config &cfg,
                                const SettingsCommitFn &on_commit,
                                const PanelClickRegion &region);
