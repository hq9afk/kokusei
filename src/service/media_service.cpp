#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <functional>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

#include "core/deferred_call.h"
#include "core/log.h"

#include "render/image.h"

#include "service/media_plugin.h"
#include "service/media_service.h"

namespace fs = std::filesystem;

namespace {

using StreamFn = decltype(&kokusei_media_plugin_stream);
using FramesFn = decltype(&kokusei_media_plugin_frames);
using ReleaseDrmFrameFn = decltype(&kokusei_media_plugin_release_drm_frame);

struct Plugin {
    StreamFn stream = nullptr;
    FramesFn frames = nullptr;
    ReleaseDrmFrameFn release_drm_frame = nullptr;
};

const Plugin &plugin() {
    static const Plugin loaded = [] {
        Plugin p;
        const char *candidates[] = {
            KOKUSEI_MEDIA_PLUGIN,
            "build/libkokusei-media.so",
        };
        void *lib = nullptr;
        for (const char *path : candidates) {
            lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
            if (lib)
                break;
        }
        if (!lib) {
            klog("media_decode: plugin not available (%s); animated content "
                 "disabled",
                 dlerror());
            return p;
        }
        p.stream = reinterpret_cast<StreamFn>(
            dlsym(lib, "kokusei_media_plugin_stream"));
        p.frames = reinterpret_cast<FramesFn>(
            dlsym(lib, "kokusei_media_plugin_frames"));
        p.release_drm_frame = reinterpret_cast<ReleaseDrmFrameFn>(
            dlsym(lib, "kokusei_media_plugin_release_drm_frame"));
        if (!p.stream || !p.frames || !p.release_drm_frame) {
            klog("media_decode: plugin missing expected symbols (%s); animated "
                 "content disabled",
                 dlerror());
            p = {};
        }
        return p;
    }();
    return loaded;
}

std::string rgba_cache_dir() {
    std::string base = animate_cache_home_dir();
    for (size_t pos = 1; pos <= base.size(); ++pos) {
        if (pos == base.size() || base[pos] == '/')
            mkdir(base.substr(0, pos).c_str(), 0755);
    }
    std::string dir = base + "/kokusei";
    mkdir(dir.c_str(), 0755);
    dir += "/wallpaper";
    mkdir(dir.c_str(), 0755);
    return dir;
}

std::string rgba_cache_path(const std::string &path, time_t mtime, int target_w,
                            int target_h) {
    std::string key = path + ":" + std::to_string(mtime) + ":" +
                      std::to_string(target_w) + "x" + std::to_string(target_h);
    size_t hash = std::hash<std::string>{}(key);
    return rgba_cache_dir() + "/" + std::to_string(hash) + ".rgba";
}

unsigned char *rgba_read(const std::string &cache_path, int &out_width,
                         int &out_height) {
    FILE *fp = fopen(cache_path.c_str(), "rb");
    if (!fp)
        return nullptr;
    uint32_t header[2];
    if (fread(header, sizeof(uint32_t), 2, fp) != 2) {
        fclose(fp);
        return nullptr;
    }
    int width = static_cast<int>(header[0]);
    int height = static_cast<int>(header[1]);
    size_t pixel_bytes = static_cast<size_t>(width) * height * 4;
    auto *data = new unsigned char[pixel_bytes];
    size_t read = fread(data, 1, pixel_bytes, fp);
    fclose(fp);
    if (read != pixel_bytes) {
        delete[] data;
        return nullptr;
    }
    out_width = width;
    out_height = height;
    return data;
}

void rgba_write(const std::string &cache_path, int width, int height,
                const unsigned char *data) {
    FILE *fp = fopen(cache_path.c_str(), "wb");
    if (!fp)
        return;
    uint32_t header[2] = {static_cast<uint32_t>(width),
                          static_cast<uint32_t>(height)};
    fwrite(header, sizeof(uint32_t), 2, fp);
    fwrite(data, 1, static_cast<size_t>(width) * height * 4, fp);
    fclose(fp);
}

void box_downsample_rgba(const unsigned char *src, int sw, int sh,
                         unsigned char *dst, int dw, int dh) {
    for (int y = 0; y < dh; ++y) {
        int y0 = static_cast<int>(static_cast<int64_t>(y) * sh / dh);
        int y1 = static_cast<int>(static_cast<int64_t>(y + 1) * sh / dh);
        y1 = std::max(y1, y0 + 1);
        for (int x = 0; x < dw; ++x) {
            int x0 = static_cast<int>(static_cast<int64_t>(x) * sw / dw);
            int x1 = static_cast<int>(static_cast<int64_t>(x + 1) * sw / dw);
            x1 = std::max(x1, x0 + 1);
            long sum[4] = {0, 0, 0, 0};
            int count = 0;
            for (int sy = y0; sy < y1 && sy < sh; ++sy) {
                const unsigned char *row =
                    src + static_cast<size_t>(sy) * sw * 4;
                for (int sx = x0; sx < x1 && sx < sw; ++sx) {
                    const unsigned char *px = row + static_cast<size_t>(sx) * 4;
                    sum[0] += px[0];
                    sum[1] += px[1];
                    sum[2] += px[2];
                    sum[3] += px[3];
                    ++count;
                }
            }
            count = std::max(count, 1);
            unsigned char *out = dst + (static_cast<size_t>(y) * dw + x) * 4;
            out[0] = static_cast<unsigned char>(sum[0] / count);
            out[1] = static_cast<unsigned char>(sum[1] / count);
            out[2] = static_cast<unsigned char>(sum[2] / count);
            out[3] = static_cast<unsigned char>(sum[3] / count);
        }
    }
}

bool is_still_image(const std::string &path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" ||
           ext == ".bmp" || ext == ".svg";
}

unsigned char *decode_first_frame(const std::string &path, int &w, int &h,
                                  int target_w = 0, int target_h = 0) {
    unsigned char *data = load_image_decode(path, w, h);
    if (data)
        return data;
    std::string filter = "format=rgba";
    if (target_w > 0 && target_h > 0)
        filter = "scale=" + std::to_string(target_w) + ":" +
                 std::to_string(target_h) +
                 ":force_original_aspect_ratio=increase,format=rgba";
    std::vector<MediaFrame> frames = media_decode_frames(path, filter, 1);
    if (frames.empty() || frames[0].rgba.empty())
        return nullptr;
    w = frames[0].width;
    h = frames[0].height;
    auto *out = new unsigned char[frames[0].rgba.size()];
    std::memcpy(out, frames[0].rgba.data(), frames[0].rgba.size());
    return out;
}

unsigned char *center_crop_square(unsigned char *src, int sw, int sh, int px,
                                  int &out_side) {
    float scale =
        std::max(static_cast<float>(px) / sw, static_cast<float>(px) / sh);
    int dw = std::max(px, static_cast<int>(std::lround(sw * scale)));
    int dh = std::max(px, static_cast<int>(std::lround(sh * scale)));
    auto *scaled = new unsigned char[static_cast<size_t>(dw) * dh * 4];
    box_downsample_rgba(src, sw, sh, scaled, dw, dh);
    delete[] src;
    auto *out = new unsigned char[static_cast<size_t>(px) * px * 4];
    int ox = (dw - px) / 2;
    int oy = (dh - px) / 2;
    for (int y = 0; y < px; ++y)
        std::memcpy(out + static_cast<size_t>(y) * px * 4,
                    scaled + (static_cast<size_t>(y + oy) * dw + ox) * 4,
                    static_cast<size_t>(px) * 4);
    delete[] scaled;
    out_side = px;
    return out;
}

int count_rgba_frames(const std::string &dir) {
    int n = 0;
    std::error_code ec;
    for (auto &e : fs::directory_iterator(dir, ec))
        if (e.path().extension() == ".rgba")
            ++n;
    return n;
}

std::string frame_path(const std::string &dir, int index) {
    char name[32];
    std::snprintf(name, sizeof(name), "/f%03d.rgba", index + 1);
    return dir + name;
}

} // namespace

MediaDecodePlayback
media_decode_stream(const std::string &path, const std::string &filter_desc,
                    int fps, bool supports_row_length,
                    MediaDecodeFrameCallback on_frame,
                    MediaDecodeDrmFrameCallback on_drm_frame) {
    if (!plugin().stream)
        return {};
    return plugin().stream(path, filter_desc, fps, supports_row_length,
                           std::move(on_frame), std::move(on_drm_frame));
}

std::vector<MediaFrame> media_decode_frames(const std::string &path,
                                            const std::string &filter_desc,
                                            int max_frames) {
    if (!plugin().frames)
        return {};
    return plugin().frames(path, filter_desc, max_frames);
}

void media_decode_stop(MediaDecodePlayback &playback) {
    if (!playback.stop_flag)
        return;
    playback.stop_flag->store(true);
    if (playback.worker.joinable())
        playback.worker.join();
    playback.stop_flag.reset();
    playback.pause_flag.reset();
    playback.status.reset();
    playback.egl_import_failed.reset();
}

void media_decode_pause(MediaDecodePlayback &playback) {
    if (playback.pause_flag)
        playback.pause_flag->store(true);
}

void media_decode_resume(MediaDecodePlayback &playback) {
    if (playback.pause_flag)
        playback.pause_flag->store(false);
}

void media_decode_release_drm_frame(void *avframe_handle) {
    if (plugin().release_drm_frame)
        plugin().release_drm_frame(avframe_handle);
}

MediaDecodeStatus media_decode_status(const MediaDecodePlayback &playback) {
    return playback.status ? playback.status->load() : MediaDecodeStatus::Blink;
}

std::string animate_cache_home_dir() {
    const char *cache_home = getenv("XDG_CACHE_HOME");
    if (cache_home && *cache_home)
        return cache_home;
    const char *home = getenv("HOME");
    return std::string(home ? home : "") + "/.cache";
}

std::string animate_cache_key(const std::string &path, long mtime) {
    return std::to_string(
        std::hash<std::string>{}(path + ":" + std::to_string(mtime)));
}

int animate_frame_index(float elapsed_s, float fps, int n) {
    if (n <= 1 || fps <= 0.0f || elapsed_s <= 0.0f)
        return 0;
    return static_cast<int>(elapsed_s * fps) % n;
}

AnimateSize animate_decode_size(int target_w, int target_h, int max_height) {
    if (target_w <= 0 || target_h <= 0 || max_height <= 0 ||
        target_h <= max_height)
        return {std::max(target_w, 0), std::max(target_h, 0)};
    double ratio = static_cast<double>(max_height) / target_h;
    return {std::max(1, static_cast<int>(std::lround(target_w * ratio))),
            max_height};
}

std::string animate_scale_filter(int w, int h, AnimateFit fit) {
    std::string ws = std::to_string(w);
    std::string hs = std::to_string(h);
    if (fit == AnimateFit::Fit)
        return "scale=" + ws + ":" + hs +
               ":force_original_aspect_ratio=decrease,pad=" + ws + ":" + hs +
               ":(ow-iw)/2:(oh-ih)/2:color=black,format=rgba";
    return "scale=" + ws + ":" + hs +
           ":force_original_aspect_ratio=increase,crop=" + ws + ":" + hs +
           ",format=rgba";
}

unsigned char *animate_decode_scaled(const std::string &path, int target_w,
                                     int target_h, int &out_width,
                                     int &out_height) {
    struct stat st{};
    bool cacheable =
        target_w > 0 && target_h > 0 && stat(path.c_str(), &st) == 0;
    std::string cache_path;
    if (cacheable) {
        cache_path = rgba_cache_path(path, st.st_mtime, target_w, target_h);
        if (unsigned char *cached =
                rgba_read(cache_path, out_width, out_height)) {
            klog("media_decode: cache hit '%s' (%dx%d)", path.c_str(),
                 out_width, out_height);
            return cached;
        }
    }

    unsigned char *data =
        decode_first_frame(path, out_width, out_height, target_w, target_h);
    if (!data)
        return nullptr;
    if (target_w <= 0 || target_h <= 0)
        return data;
    float scale = std::max(static_cast<float>(target_w) / out_width,
                           static_cast<float>(target_h) / out_height);
    unsigned char *result = data;
    if (scale < 1.0f) {
        int dw = std::max(1, static_cast<int>(std::lround(out_width * scale)));
        int dh = std::max(1, static_cast<int>(std::lround(out_height * scale)));
        auto *scaled = new unsigned char[static_cast<size_t>(dw) * dh * 4];
        box_downsample_rgba(data, out_width, out_height, scaled, dw, dh);
        delete[] data;
        out_width = dw;
        out_height = dh;
        result = scaled;
    }
    if (cacheable)
        rgba_write(cache_path, out_width, out_height, result);
    return result;
}

unsigned char *animate_job_frame(const AnimateJob &job, int index,
                                 int &out_width, int &out_height) {
    if (index < 0 || index >= job.frame_count || job.frames_dir.empty())
        return nullptr;
    return rgba_read(frame_path(job.frames_dir, index), out_width, out_height);
}

void animate_job_start(AnimateJob &job, const std::string &source_path,
                       const AnimateDecodeParams &params,
                       std::function<void()> on_ready) {
    if (job.attempted)
        return;
    job.attempted = true;

    struct stat st{};
    if (source_path.empty() || stat(source_path.c_str(), &st) != 0) {
        klog("media_decode: no source media '%s'", source_path.c_str());
        return;
    }

    std::string key = animate_cache_key(source_path, st.st_mtime);
    std::string dir = animate_cache_home_dir() + "/kokusei/animated/" + key;
    int px = std::max(1, params.square_px);
    int fps = std::max(1, params.fps);
    bool still = is_still_image(source_path);
    int max_frames = still ? 1 : fps * kAnimateMaxSeconds;

    job.worker = std::thread([&job, source_path, dir, px, fps, max_frames,
                              still, on_ready = std::move(on_ready)] {
        std::error_code ec;
        int existing = count_rgba_frames(dir);
        if (existing == 0) {
            std::string tmp = dir + ".tmp";
            fs::remove_all(tmp, ec);
            fs::create_directories(tmp, ec);
            if (still) {
                int w = 0, h = 0;
                unsigned char *data =
                    decode_first_frame(source_path, w, h, px, px);
                if (data) {
                    int side = 0;
                    unsigned char *sq =
                        center_crop_square(data, w, h, px, side);
                    rgba_write(tmp + "/f001.rgba", side, side, sq);
                    delete[] sq;
                }
            } else {
                std::string filter =
                    "fps=" + std::to_string(fps) + "," +
                    animate_scale_filter(px, px, AnimateFit::Crop);
                std::vector<MediaFrame> frames =
                    media_decode_frames(source_path, filter, max_frames);
                for (size_t i = 0; i < frames.size(); ++i) {
                    char name[32];
                    std::snprintf(name, sizeof(name), "/f%03zu.rgba", i + 1);
                    rgba_write(tmp + name, frames[i].width, frames[i].height,
                               frames[i].rgba.data());
                }
            }
            existing = count_rgba_frames(tmp);
            if (existing > 0) {
                fs::remove_all(dir, ec);
                fs::rename(tmp, dir, ec);
                if (ec)
                    klog("media_decode: frame promotion failed for '%s': %s",
                         source_path.c_str(), ec.message().c_str());
            } else {
                fs::remove_all(tmp, ec);
            }
        }
        if (existing <= 0) {
            klog("media_decode: decode produced no frames for '%s'",
                 source_path.c_str());
            return;
        }
        job.frames_dir = dir;
        job.frame_count = existing;
        DeferredCall::call_later([&job, on_ready] {
            job.ready = true;
            if (on_ready)
                on_ready();
        });
    });
}
