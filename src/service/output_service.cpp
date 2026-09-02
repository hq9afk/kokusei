#include "service/output_service.h"

wl_output *active_output_select(const std::vector<Output *> &outputs,
                                const std::string &focused_name,
                                wl_output *pointer_hint) {
    if (!focused_name.empty()) {
        for (Output *o : outputs)
            if (o->name == focused_name)
                return o->wl;
    }
    if (pointer_hint)
        return pointer_hint;
    return outputs.empty() ? nullptr : outputs.front()->wl;
}

namespace {

void output_scale_preferred(void *data, wl_surface *surface, int32_t scale) {
    auto *state = static_cast<OutputScale *>(data);
    if (scale <= 0 || scale == state->scale)
        return;
    state->scale = scale;
    wl_surface_set_buffer_scale(surface, scale);
    if (state->on_change)
        state->on_change(scale);
}

const wl_surface_listener &output_scale_listener() {
    static constexpr wl_surface_listener l{
        .enter = [](void *, wl_surface *, wl_output *) {},
        .leave = [](void *, wl_surface *, wl_output *) {},
        .preferred_buffer_scale = output_scale_preferred,
        .preferred_buffer_transform = [](void *, wl_surface *, uint32_t) {},
    };
    return l;
}

} // namespace

void output_scale_watch(OutputScale &state, wl_surface *surface) {
    wl_surface_add_listener(surface, &output_scale_listener(), &state);
}
