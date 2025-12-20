#pragma once
#include <string>

#ifdef MMT_ENABLE_OPENCV
#include <opencv2/opencv.hpp>
#endif

class Camera {
public:
    Camera();
    ~Camera();

    bool open(int index = 0);
    bool isOpened() const;
    void close();

    // Chụp 1 frame (đã có)
    bool captureFrame(std::string& out_base64_jpeg);

    // QUAY VIDEO: durationSeconds ~10s, trả về base64 của file video
    bool captureVideo(int durationSeconds,
                      std::string& out_base64_video,
                      std::string& out_format); // ví dụ "avi"

private:
#ifdef MMT_ENABLE_OPENCV
    cv::VideoCapture cap;
#endif
};
