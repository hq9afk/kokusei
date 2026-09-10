#pragma once

#include "modules/settings.h"

void rain_tab_paint(SettingsState &state, Node *root, int32_t scale, float x,
                    float y, float w, const Config &cfg);

bool rain_tab_handle_click(SettingsState &state, const Config &cfg,
                           const SettingsCommitFn &on_commit,
                           const PanelClickRegion &region);
