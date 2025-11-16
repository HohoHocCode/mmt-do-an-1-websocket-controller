#pragma once

#include <opencv2/core.hpp>
#include <vector>

class ScreenCapturer {
public:
    ScreenCapturer() = default;

    // Hàm chung – auto detect OS
    cv::Mat captureScreen();

    // Encode JPEG
    std::vector<uchar> encodeJpeg(const cv::Mat& img, int quality = 80);

private:
    cv::Mat captureDummy();

#if defined(__linux__)
    cv::Mat captureScreenLinux();
#endif

#if defined(_WIN32)
    cv::Mat captureScreenWindows();
#endif

#if defined(__APPLE__)
    cv::Mat captureScreenMac();
#endif
};
