#pragma once

#include "modules/trulla.h"

void starward_tab_paint(TrullaState &state, Node *root, int32_t scale, float x,
                        float y, float w, const Config &cfg);

bool starward_tab_handle_click(TrullaState &state, const Config &cfg,
                               const TrullaCommitFn &on_commit,
                               const PanelClickRegion &region);
