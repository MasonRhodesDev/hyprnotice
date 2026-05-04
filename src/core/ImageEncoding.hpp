#pragma once

#include <cstdint>
#include <vector>

#include "Notification.hpp"

namespace HN {

    // Encode raw freedesktop image-data (R,G,B[,A] bytes, non-premultiplied,
    // top-down) as PNG bytes suitable for CImageBuilder::data(). Returns
    // empty on failure. Implementation uses vendored stb_image_write — no
    // new system deps.
    std::vector<uint8_t> encodeImageDataToPng(const SNotification::SImageData& img);

}
