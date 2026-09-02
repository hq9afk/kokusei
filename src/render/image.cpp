#include <cstdio>
#include <jpeglib.h>
#include <librsvg/rsvg.h>
#include <png.h>
#include <string_view>
#include <vector>

#include "core/log.h"

#include "render/image.h"

namespace {

unsigned char *decode_png(FILE *fp, int &width, int &height) {
    png_byte header[8];
    if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8))
        return nullptr;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                             nullptr, nullptr);
    if (!png)
        return nullptr;
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        return nullptr;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        return nullptr;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    width = static_cast<int>(png_get_image_width(png, info));
    height = static_cast<int>(png_get_image_height(png, info));
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16)
        png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    auto *data = new unsigned char[static_cast<size_t>(width) * height * 4];
    std::vector<png_bytep> rows(static_cast<size_t>(height));
    for (int y = 0; y < height; ++y)
        rows[static_cast<size_t>(y)] =
            data + static_cast<size_t>(y) * width * 4;
    png_read_image(png, rows.data());

    png_destroy_read_struct(&png, &info, nullptr);
    return data;
}

unsigned char *decode_jpeg(FILE *fp, int &width, int &height) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return nullptr;
    }

#ifdef JCS_EXTENSIONS
    cinfo.out_color_space = JCS_EXT_RGBA;
#else
    cinfo.out_color_space = JCS_RGB;
#endif
    jpeg_start_decompress(&cinfo);
    width = static_cast<int>(cinfo.output_width);
    height = static_cast<int>(cinfo.output_height);

    auto *data = new unsigned char[static_cast<size_t>(width) * height * 4];
#ifdef JCS_EXTENSIONS
    while (cinfo.output_scanline < cinfo.output_height) {
        unsigned char *row =
            data + static_cast<size_t>(cinfo.output_scanline) * width * 4;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }
#else
    std::vector<unsigned char> row_buf(static_cast<size_t>(width) * 3);
    while (cinfo.output_scanline < cinfo.output_height) {
        int y = static_cast<int>(cinfo.output_scanline);
        unsigned char *row_ptr = row_buf.data();
        jpeg_read_scanlines(&cinfo, &row_ptr, 1);
        unsigned char *out = data + static_cast<size_t>(y) * width * 4;
        for (int x = 0; x < width; ++x) {
            out[x * 4 + 0] = row_buf[static_cast<size_t>(x) * 3 + 0];
            out[x * 4 + 1] = row_buf[static_cast<size_t>(x) * 3 + 1];
            out[x * 4 + 2] = row_buf[static_cast<size_t>(x) * 3 + 2];
            out[x * 4 + 3] = 0xFF;
        }
    }
#endif
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return data;
}

bool looks_like_svg(const unsigned char *sig, size_t n) {
    std::string_view head(reinterpret_cast<const char *>(sig), n);
    return head.find("<svg") != std::string_view::npos ||
           head.find("<?xml") != std::string_view::npos;
}

unsigned char *decode_svg(const std::string &path, int target_px, int &width,
                          int &height) {
    GError *error = nullptr;
    RsvgHandle *handle = rsvg_handle_new_from_file(path.c_str(), &error);
    if (!handle) {
        klog("svg: failed to open '%s': %s", path.c_str(),
             error ? error->message : "unknown error");
        if (error)
            g_error_free(error);
        return nullptr;
    }

    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, target_px, target_px);
    cairo_t *cr = cairo_create(surface);
    RsvgRectangle viewport = {0.0, 0.0, static_cast<double>(target_px),
                              static_cast<double>(target_px)};
    bool ok = rsvg_handle_render_document(handle, cr, &viewport, &error);
    cairo_surface_flush(surface);

    unsigned char *out = nullptr;
    if (ok) {
        int stride = cairo_image_surface_get_stride(surface);
        const unsigned char *src = cairo_image_surface_get_data(surface);
        out = new unsigned char[static_cast<size_t>(target_px) * target_px * 4];
        for (int y = 0; y < target_px; ++y) {
            const auto *row =
                reinterpret_cast<const uint32_t *>(src + y * stride);
            unsigned char *out_row =
                out + static_cast<size_t>(y) * target_px * 4;
            for (int x = 0; x < target_px; ++x) {
                uint32_t px = row[x];
                uint8_t a = (px >> 24) & 0xFF;
                uint8_t r = (px >> 16) & 0xFF;
                uint8_t g = (px >> 8) & 0xFF;
                uint8_t b = px & 0xFF;
                if (a != 0 && a != 255) {
                    r = static_cast<uint8_t>(r * 255 / a);
                    g = static_cast<uint8_t>(g * 255 / a);
                    b = static_cast<uint8_t>(b * 255 / a);
                }
                out_row[x * 4 + 0] = r;
                out_row[x * 4 + 1] = g;
                out_row[x * 4 + 2] = b;
                out_row[x * 4 + 3] = a;
            }
        }
        width = height = target_px;
    } else {
        klog("svg: failed to render '%s': %s", path.c_str(),
             error ? error->message : "unknown error");
        if (error)
            g_error_free(error);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(handle);
    return out;
}

} // namespace

unsigned char *load_image_decode(const std::string &path, int &width,
                                 int &height, int svg_target_px) {
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) {
        klog("image: failed to open '%s'", path.c_str());
        return nullptr;
    }

    unsigned char sig[64] = {0};
    size_t n = fread(sig, 1, sizeof(sig), fp);
    fseek(fp, 0, SEEK_SET);

    unsigned char *data = nullptr;
    if (n >= 8 && !png_sig_cmp(sig, 0, 8)) {
        data = decode_png(fp, width, height);
    } else if (n >= 2 && sig[0] == 0xFF && sig[1] == 0xD8) {
        data = decode_jpeg(fp, width, height);
    } else if (looks_like_svg(sig, n)) {
        fclose(fp);
        if (svg_target_px <= 0) {
            klog("image: '%s' is an SVG but no target size was given",
                 path.c_str());
            return nullptr;
        }
        data = decode_svg(path, svg_target_px, width, height);
        if (!data)
            klog("image: failed to load '%s'", path.c_str());
        return data;
    }

    fclose(fp);
    if (!data)
        klog("image: failed to load '%s'", path.c_str());
    return data;
}

Texture load_image_texture(const std::string &path, int svg_target_px) {
    int width = 0, height = 0;
    unsigned char *data = load_image_decode(path, width, height, svg_target_px);
    if (!data)
        return Texture{};
    Texture tex = make_texture_rgba(width, height, data, true);
    delete[] data;
    return tex;
}
