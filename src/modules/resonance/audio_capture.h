#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include <pipewire/pipewire.h>
#include <spa/param/audio/raw.h>
#include <spa/utils/hook.h>

class ResonanceAudioCapture {
  public:
    ~ResonanceAudioCapture();

    bool start();
    void stop();

    bool take(std::vector<float> &l, std::vector<float> &r, bool &modified);

  private:
    static void on_process(void *data);
    static void on_param_changed(void *data, uint32_t id, const spa_pod *param);
    static void on_state_changed(void *data, pw_stream_state old_state,
                                 pw_stream_state state, const char *error);

    void process_buffer();

    pw_thread_loop *loop_ = nullptr;
    pw_context *context_ = nullptr;
    pw_core *core_ = nullptr;
    pw_stream *stream_ = nullptr;
    spa_hook stream_listener_{};
    int channels_ = 2;

    std::mutex mutex_;
    std::vector<float> audio_buffer_;
    std::vector<float> br_, bl_;
    std::vector<float> rb_, lb_;
    int filled_idx_ = 0;
    bool modified_ = false;
    bool have_data_ = false;
};
