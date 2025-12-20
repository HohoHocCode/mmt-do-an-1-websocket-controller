#include "doctest/doctest.h"
#include "utils/limits.hpp"

TEST_CASE("download chunk clamp respects bounds") {
    using namespace limits;

    CHECK(clamp_download_chunk_bytes(1) == kMinDownloadChunkBytes);
    CHECK(clamp_download_chunk_bytes(kMinDownloadChunkBytes + 512) == kMinDownloadChunkBytes + 512);
    CHECK(clamp_download_chunk_bytes(kMaxDownloadChunkBytes + 1) == kMaxDownloadChunkBytes);
}

TEST_CASE("stream config clamp keeps values in range") {
    using namespace limits;

    CHECK(clamp_stream_fps(0) == 1);
    CHECK(clamp_stream_fps(120) == 30);

    CHECK(clamp_stream_jpeg_quality(10) == 30);
    CHECK(clamp_stream_jpeg_quality(120) == 95);
}
