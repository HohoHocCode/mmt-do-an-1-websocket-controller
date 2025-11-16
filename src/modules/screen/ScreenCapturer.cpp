#include "modules/screen/ScreenCapturer.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <spdlog/spdlog.h>

cv::Mat ScreenCapturer::captureScreen() {
#if defined(__linux__)
    return captureScreenLinux();
#elif defined(_WIN32)
    return captureScreenWindows();
#elif defined(__APPLE__)
    return captureScreenMac();
#else
    return captureDummy();
#endif
}

cv::Mat ScreenCapturer::captureDummy() {
    int width = 800, height = 600;
    cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 255, 0));

    cv::putText(img, "Dummy Screenshot",
                {50, height / 2},
                cv::FONT_HERSHEY_SIMPLEX,
                1.5, {0, 0, 0}, 3);

    return img;
}

std::vector<uchar> ScreenCapturer::encodeJpeg(const cv::Mat& img, int quality) {
    std::vector<uchar> out;
    std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, quality };
    cv::imencode(".jpg", img, out, params);
    return out;
}
