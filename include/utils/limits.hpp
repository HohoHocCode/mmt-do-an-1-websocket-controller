#pragma once

#include <algorithm>
#include <cstddef>

namespace limits {
constexpr std::size_t kMinDownloadChunkBytes = 4096;
constexpr std::size_t kMaxDownloadChunkBytes = 262144;
constexpr std::size_t kMaxMessageBytes = 256 * 1024;

inline std::size_t clamp_download_chunk_bytes(std::size_t requested) {
    return std::min(std::max(requested, kMinDownloadChunkBytes), kMaxDownloadChunkBytes);
}

inline int clamp_stream_fps(int fps) {
    return std::clamp(fps, 1, 30);
}

inline int clamp_stream_jpeg_quality(int quality) {
    return std::clamp(quality, 30, 95);
}

inline int clamp_stream_max_width(int width) {
    return std::clamp(width, 0, 7680);
}

inline int clamp_stream_max_height(int height) {
    return std::clamp(height, 0, 4320);
}
} // namespace limits
