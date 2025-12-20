#pragma once
#include <string>
#include <cstddef>

struct ScreenCaptureOptions {
    int jpeg_quality = 80;
    int max_width = 0;
    int max_height = 0;
};

struct ScreenCaptureResult {
    std::string base64;
    int width = 0;
    int height = 0;
    double capture_ms = 0.0;
    double encode_ms = 0.0;
    std::size_t bytes = 0;
    bool resized = false;
};

class ScreenCapture {
public:
    static ScreenCaptureResult capture_base64(const ScreenCaptureOptions& options);
    static bool supports_resize();
};
