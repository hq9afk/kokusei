#pragma once

#include <vector>

#include "service/input_service.h"

struct WaylandState;

void dispatch_key_events(WaylandState &state,
                         const std::vector<KeyEvent> &events);
