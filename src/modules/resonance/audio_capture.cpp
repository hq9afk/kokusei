#include <array>
#include <cstring>

#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/format.h>
#include <spa/param/audio/raw-utils.h>
#include <spa/param/format-utils.h>
#include <spa/pod/builder.h>

#include "config/resonance_config.h"

#include "core/log.h"

#include "modules/resonance/audio_capture.h"

namespace {

constexpr int kHalfSample = kResonanceSampleSize / 2;
constexpr int kQuarterSample = kResonanceSampleSize / 4;
constexpr int kSlide = kQuarterSample + kQuarterSample;
constexpr int kTail = kResonanceFragmentSize - kQuarterSample - kQuarterSample;

} // namespace

ResonanceAudioCapture::~ResonanceAudioCapture() { stop(); }

bool ResonanceAudioCapture::start() {
    if (loop_)
        return true;

    audio_buffer_.assign(static_cast<size_t>(kHalfSample + 1), 0.0f);
    br_.assign(static_cast<size_t>(kResonanceSampleSize * 4), 0.0f);
    bl_.assign(static_cast<size_t>(kResonanceSampleSize * 4), 0.0f);
    rb_.assign(static_cast<size_t>(kResonanceFragmentSize), 0.0f);
    lb_.assign(static_cast<size_t>(kResonanceFragmentSize), 0.0f);
    filled_idx_ = 0;
    modified_ = false;
    have_data_ = false;

    pw_init(nullptr, nullptr);
    loop_ = pw_thread_loop_new("kokusei-resonance-audio", nullptr);
    if (!loop_) {
        klog("resonance_audio: failed to create pw_thread_loop");
        return false;
    }
    context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
    core_ = pw_context_connect(context_, nullptr, 0);
    if (!core_) {
        klog("resonance_audio: failed to connect pipewire core");
        return false;
    }

    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music", PW_KEY_MEDIA_NAME, "kokusei resonance",
        PW_KEY_NODE_ALWAYS_PROCESS, "true", PW_KEY_STREAM_CAPTURE_SINK, "true",
        nullptr);

    pw_thread_loop_lock(loop_);

    stream_ = pw_stream_new(core_, "kokusei-resonance", props);
    static const pw_stream_events events = [] {
        pw_stream_events e{};
        e.version = PW_VERSION_STREAM_EVENTS;
        e.state_changed = &ResonanceAudioCapture::on_state_changed;
        e.param_changed = &ResonanceAudioCapture::on_param_changed;
        e.process = &ResonanceAudioCapture::on_process;
        return e;
    }();
    pw_stream_add_listener(stream_, &stream_listener_, &events, this);

    std::array<uint8_t, 512> buf{};
    spa_pod_builder b;
    spa_pod_builder_init(&b, buf.data(), static_cast<uint32_t>(buf.size()));
    spa_audio_info_raw raw{};
    raw.format = SPA_AUDIO_FORMAT_F32_LE;
    raw.rate = kResonanceSampleRate;
    raw.channels = static_cast<uint32_t>(kResonanceChannels);
    const spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &raw);

    pw_stream_connect(stream_, PW_DIRECTION_INPUT, PW_ID_ANY,
                      static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                   PW_STREAM_FLAG_MAP_BUFFERS |
                                                   PW_STREAM_FLAG_RT_PROCESS),
                      params, 1);

    pw_thread_loop_unlock(loop_);
    pw_thread_loop_start(loop_);
    return true;
}

void ResonanceAudioCapture::stop() {
    if (loop_)
        pw_thread_loop_stop(loop_);
    if (stream_) {
        pw_stream_destroy(stream_);
        stream_ = nullptr;
    }
    if (core_) {
        pw_core_disconnect(core_);
        core_ = nullptr;
    }
    if (context_) {
        pw_context_destroy(context_);
        context_ = nullptr;
    }
    if (loop_) {
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
    }
}

void ResonanceAudioCapture::on_state_changed(void *, pw_stream_state,
                                             pw_stream_state state,
                                             const char *error) {
    if (state == PW_STREAM_STATE_ERROR)
        klog("resonance_audio: stream error: %s", error ? error : "unknown");
}

void ResonanceAudioCapture::on_param_changed(void *data, uint32_t id,
                                             const spa_pod *param) {
    auto *self = static_cast<ResonanceAudioCapture *>(data);
    if (!param || id != SPA_PARAM_Format)
        return;

    spa_audio_info info{};
    if (spa_format_parse(param, &info.media_type, &info.media_subtype) < 0)
        return;
    if (info.media_type != SPA_MEDIA_TYPE_audio ||
        info.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    spa_audio_info_raw raw{};
    if (spa_format_audio_raw_parse(param, &raw) < 0 || raw.channels == 0)
        return;
    self->channels_ = static_cast<int>(raw.channels);
}

void ResonanceAudioCapture::on_process(void *data) {
    static_cast<ResonanceAudioCapture *>(data)->process_buffer();
}

void ResonanceAudioCapture::process_buffer() {
    if (!stream_)
        return;

    pw_buffer *pwb = pw_stream_dequeue_buffer(stream_);
    if (!pwb)
        return;

    spa_buffer *sbuf = pwb->buffer;
    const float *samples = nullptr;
    int n_samples = 0;
    if (sbuf && sbuf->n_datas > 0) {
        spa_data *d = &sbuf->datas[0];
        if (d->data && d->chunk) {
            const auto *base =
                static_cast<const uint8_t *>(d->data) + d->chunk->offset;
            samples = reinterpret_cast<const float *>(base);
            n_samples = static_cast<int>(d->chunk->size / sizeof(float));
        }
    }

    if (samples) {
        std::lock_guard<std::mutex> lock(mutex_);
        int n_channels = channels_;
        for (int n = 0; n < n_samples; ++n) {
            if (filled_idx_ <= kHalfSample) {
                audio_buffer_[static_cast<size_t>(filled_idx_)] = samples[n];
                ++filled_idx_;
                continue;
            }

            std::memmove(br_.data(), br_.data() + kSlide,
                         static_cast<size_t>(kResonanceFragmentSize - kSlide) *
                             sizeof(float));
            std::memmove(bl_.data(), bl_.data() + kSlide,
                         static_cast<size_t>(kResonanceFragmentSize - kSlide) *
                             sizeof(float));

            for (int k = 0, i = 0; i < kHalfSample; i += 2, k += 2) {
                int idx = kTail + k;
                if (n_channels == 1) {
                    float s = (audio_buffer_[static_cast<size_t>(i)] +
                               audio_buffer_[static_cast<size_t>(i + 1)]) /
                              2.0f;
                    br_[static_cast<size_t>(idx)] = s;
                    bl_[static_cast<size_t>(idx)] = s;
                } else {
                    br_[static_cast<size_t>(idx)] =
                        audio_buffer_[static_cast<size_t>(i + 1)];
                    bl_[static_cast<size_t>(idx)] =
                        audio_buffer_[static_cast<size_t>(i)];
                }
                br_[static_cast<size_t>(idx + 1)] = 0.0f;
                bl_[static_cast<size_t>(idx + 1)] = 0.0f;
            }

            filled_idx_ = 0;
            std::memcpy(rb_.data(), br_.data(),
                        static_cast<size_t>(kResonanceFragmentSize) *
                            sizeof(float));
            std::memcpy(lb_.data(), bl_.data(),
                        static_cast<size_t>(kResonanceFragmentSize) *
                            sizeof(float));
            modified_ = true;
            have_data_ = true;
        }
    }

    pw_stream_queue_buffer(stream_, pwb);
}

bool ResonanceAudioCapture::take(std::vector<float> &l, std::vector<float> &r,
                                 bool &modified) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!have_data_) {
        modified = false;
        return false;
    }
    modified = modified_;
    modified_ = false;
    if (modified) {
        l.assign(lb_.begin(), lb_.end());
        r.assign(rb_.begin(), rb_.end());
    }
    return true;
}
