#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using MediaDecodeFrameCallback = std::function<void(
    unsigned char *rgba, int width, int height, int stride_px)>;

struct MediaDrmPlane {
    int fd = -1;
    uint64_t modifier = 0;
    int offset = 0;
    int pitch = 0;
};

struct MediaDrmFrame {
    MediaDrmPlane planes[2];
    int plane_count = 0;
    int width = 0;
    int height = 0;
    void *avframe_handle = nullptr;
};

using MediaDecodeDrmFrameCallback = std::function<void(MediaDrmFrame frame)>;

enum class MediaDecodeStatus { Blink, ZeroCopy, CpuFallback };

struct MediaDecodePlayback {
    std::thread worker;
    std::shared_ptr<std::atomic<bool>> stop_flag;
    std::shared_ptr<std::atomic<bool>> pause_flag;
    std::shared_ptr<std::atomic<MediaDecodeStatus>> status;
    std::shared_ptr<std::atomic<bool>> egl_import_failed;

    MediaDecodePlayback() = default;
    MediaDecodePlayback(const MediaDecodePlayback &) = delete;
    MediaDecodePlayback &operator=(const MediaDecodePlayback &) = delete;
    MediaDecodePlayback(MediaDecodePlayback &&) = default;
    MediaDecodePlayback &operator=(MediaDecodePlayback &&other) noexcept {
        if (this != &other) {
            join();
            worker = std::move(other.worker);
            stop_flag = std::move(other.stop_flag);
            pause_flag = std::move(other.pause_flag);
            status = std::move(other.status);
            egl_import_failed = std::move(other.egl_import_failed);
        }
        return *this;
    }
    ~MediaDecodePlayback() { join(); }

  private:
    void join() {
        if (stop_flag)
            stop_flag->store(true);
        if (worker.joinable())
            worker.join();
    }
};

MediaDecodePlayback
media_decode_stream(const std::string &path, const std::string &filter_desc,
                    int fps, bool supports_row_length,
                    MediaDecodeFrameCallback on_frame,
                    MediaDecodeDrmFrameCallback on_drm_frame = nullptr);

void media_decode_stop(MediaDecodePlayback &playback);
void media_decode_pause(MediaDecodePlayback &playback);
void media_decode_resume(MediaDecodePlayback &playback);

MediaDecodeStatus media_decode_status(const MediaDecodePlayback &playback);

void media_decode_release_drm_frame(void *avframe_handle);

struct MediaFrame {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
};

std::vector<MediaFrame> media_decode_frames(const std::string &path,
                                            const std::string &filter_desc,
                                            int max_frames);

constexpr int kAnimateMaxSeconds = 30;
inline constexpr int kAnimateExpanseFps = 15;
inline constexpr int kAnimateMaxDecodeDim = 1440;

enum class AnimateFit { Crop, Fit };

struct AnimateDecodeParams {
    int fps = 15;
    int square_px = 200;
};

struct AnimateJob {
    std::string frames_dir;
    int frame_count = 0;
    bool ready = false;
    bool attempted = false;
    std::thread worker;

    AnimateJob() = default;
    AnimateJob(const AnimateJob &) = delete;
    AnimateJob &operator=(const AnimateJob &) = delete;

    ~AnimateJob() {
        if (worker.joinable())
            worker.join();
    }
};

void animate_job_start(AnimateJob &job, const std::string &source_path,
                       const AnimateDecodeParams &params,
                       std::function<void()> on_ready);

unsigned char *animate_job_frame(const AnimateJob &job, int index,
                                 int &out_width, int &out_height);

std::string animate_cache_home_dir();

std::string animate_cache_key(const std::string &path, long mtime);

int animate_frame_index(float elapsed_s, float fps, int n);

struct AnimateSize {
    int w = 0;
    int h = 0;
};

AnimateSize animate_decode_size(int target_w, int target_h, int max_height);

std::string animate_scale_filter(int w, int h, AnimateFit fit);

unsigned char *animate_decode_scaled(const std::string &path, int target_w,
                                     int target_h, int &out_width,
                                     int &out_height);
