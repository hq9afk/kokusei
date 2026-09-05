#pragma once

#include <string>

#include "modules/trulla.h"

std::string resonance_field_text(const ResonanceParams &params,
                                 TrullaFieldId id);

void resonance_tab_paint(TrullaState &state, Node *root, int32_t scale, float x,
                         float y, float w, const Config &cfg);

bool resonance_tab_handle_click(TrullaState &state, const Config &cfg,
                                const TrullaCommitFn &on_commit,
                                const PanelClickRegion &region);
