#include "modules/screen.hpp"
#include "utils/base64.hpp"
#include "utils/limits.hpp"

#if defined(_WIN32) && defined(MMT_ENABLE_OPENCV)
#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>

#include <opencv2/opencv.hpp>

namespace {
cv::Size compute_target_size(int width, int height, int max_width, int max_height, bool& resized) {
    resized = false;
    if (max_width <= 0 && max_height <= 0) {
        return {width, height};
    }

    double scale_w = max_width > 0 ? static_cast<double>(max_width) / width : 1.0;
    double scale_h = max_height > 0 ? static_cast<double>(max_height) / height : 1.0;
    double scale = std::min({scale_w, scale_h, 1.0});

    int target_w = static_cast<int>(width * scale);
    int target_h = static_cast<int>(height * scale);
    if (target_w <= 0 || target_h <= 0) {
        return {width, height};
    }

    if (target_w != width || target_h != height) {
        resized = true;
    }
    return {target_w, target_h};
}
} // namespace

ScreenCaptureResult ScreenCapture::capture_base64(const ScreenCaptureOptions& options)
{
    ScreenCaptureResult result;
    int width  = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    if (width <= 0 || height <= 0) {
        return result;
    }

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    SelectObject(hMemoryDC, hBitmap);

    const auto capture_start = std::chrono::steady_clock::now();
    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    std::vector<BYTE> buffer(width * height * 3);

    const int scanlines = GetDIBits(
        hMemoryDC,
        hBitmap,
        0,
        height,
        buffer.data(),
        (BITMAPINFO*)&bi,
        DIB_RGB_COLORS
    );
    const auto capture_end = std::chrono::steady_clock::now();

    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    if (scanlines == 0) {
        return result;
    }

    cv::Mat img(height, width, CV_8UC3, buffer.data());
    cv::Mat resized_img;
    bool resized = false;
    cv::Size target_size = compute_target_size(width, height, options.max_width, options.max_height, resized);
    if (resized) {
        cv::resize(img, resized_img, target_size, 0, 0, cv::INTER_AREA);
    }

    const auto encode_start = std::chrono::steady_clock::now();
    std::vector<uchar> encoded;
    int quality = limits::clamp_stream_jpeg_quality(options.jpeg_quality);
    cv::imencode(".jpg", resized ? resized_img : img, encoded, { cv::IMWRITE_JPEG_QUALITY, quality });

    result.base64 = base64_encode(encoded.data(), encoded.size());
    const auto encode_end = std::chrono::steady_clock::now();

    result.width = resized ? target_size.width : width;
    result.height = resized ? target_size.height : height;
    result.resized = resized;
    result.bytes = encoded.size();
    result.capture_ms = std::chrono::duration<double, std::milli>(capture_end - capture_start).count();
    result.encode_ms = std::chrono::duration<double, std::milli>(encode_end - encode_start).count();
    return result;
}

bool ScreenCapture::supports_resize() {
    return true;
}
#else
ScreenCaptureResult ScreenCapture::capture_base64(const ScreenCaptureOptions&) {
    return {};
}

bool ScreenCapture::supports_resize() {
    return false;
}
#endif
