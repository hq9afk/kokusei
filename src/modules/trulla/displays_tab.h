#pragma once

#include "modules/trulla.h"

void trulla_draw_monitor_row(TrullaState &state, Node *parent, int32_t scale,
                             float x, float y, float row_w,
                             const std::string &selected_monitor);

void displays_tab_paint(TrullaState &state, Node *root, int32_t scale, float x,
                        float y, float w, const Config &cfg);

bool displays_tab_handle_click(TrullaState &state, const Config &cfg,
                               const TrullaCommitFn &on_commit,
                               const PanelClickRegion &region);
