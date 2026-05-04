#include "ImageEncoding.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../vendor/stb_image_write.h"

#include "../helpers/Log.hpp"

namespace HN {

    namespace {
        // stb's write_png_to_mem writes via a callback. Collect into a vector.
        void appendCallback(void* user, void* data, int size) {
            auto& out = *static_cast<std::vector<uint8_t>*>(user);
            const auto* p = static_cast<const uint8_t*>(data);
            out.insert(out.end(), p, p + size);
        }
    }

    std::vector<uint8_t> encodeImageDataToPng(const SNotification::SImageData& img) {
        if (img.empty() || img.width <= 0 || img.height <= 0)
            return {};

        const int channels = img.channels;
        if (channels != 3 && channels != 4) {
            Debug::log(Debug::WARN, "image: unsupported channels={}", channels);
            return {};
        }
        if (img.bitsPerSample != 8) {
            Debug::log(Debug::WARN, "image: unsupported bits_per_sample={}", img.bitsPerSample);
            return {};
        }

        const int expectedStride = img.width * channels;
        const int stride = img.rowstride > 0 ? img.rowstride : expectedStride;

        std::vector<uint8_t> out;
        out.reserve(img.width * img.height * channels / 2);

        // stbi_write_png_to_func writes a single PNG. Stride is in bytes.
        const int rc = stbi_write_png_to_func(
            appendCallback, &out,
            img.width, img.height, channels,
            img.data.data(), stride);

        if (!rc) {
            Debug::log(Debug::WARN, "image: stbi_write_png failed (w={}, h={}, ch={})",
                       img.width, img.height, channels);
            return {};
        }
        Debug::log(Debug::TRACE, "image: encoded {}x{} ({} ch) to {} byte PNG",
                   img.width, img.height, channels, out.size());
        return out;
    }

}
