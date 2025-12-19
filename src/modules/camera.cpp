#include "modules/camera.hpp"
#include "utils/base64.hpp"

#include <vector>
#include <fstream>
#include <cstdio>   // std::remove

Camera::Camera() {}

Camera::~Camera() {
    close();
}

bool Camera::open(int index) {
    if (cap.isOpened())
        return true;
    return cap.open(index);
}

bool Camera::isOpened() const {
    return cap.isOpened();
}

void Camera::close() {
    if (cap.isOpened())
        cap.release();
}

bool Camera::capture_frame(std::string& out_base64_jpeg)
{
    if (!cap.isOpened()) {
        if (!cap.open(0))
            return false;
    }

    cv::Mat frame;
    if (!cap.read(frame))
        return false;

    std::vector<uchar> buf;
    if (!cv::imencode(".jpg", frame, buf))
        return false;

    out_base64_jpeg = base64_encode(buf.data(), buf.size());
    return true;
}

// QUAY VIDEO ~durationSeconds, trả base64 video (offline)
bool Camera::capture_video(int durationSeconds,
                           std::string& out_base64_video,
                           std::string& out_format)
{
    if (durationSeconds <= 0)
        return false;

    // Mở camera nếu chưa mở
    if (!cap.isOpened()) {
        if (!cap.open(0))
            return false;
    }

    // Đọc 1 frame để lấy kích thước
    cv::Mat frame;
    if (!cap.read(frame))
        return false;

    int fps = 15;  // tạm thời, cho nhẹ
    int totalFrames = fps * durationSeconds;
    cv::Size size = frame.size();

    // File tạm để ghi video
    const std::string filename = "camera_temp.avi";
    out_format = "avi";

    // MJPG cho đơn giản
    int fourcc = cv::VideoWriter::fourcc('M','J','P','G');
    cv::VideoWriter writer(filename, fourcc, fps, size, true);

    if (!writer.isOpened())
        return false;

    // Viết frame đầu tiên
    writer.write(frame);

    // Ghi thêm các frame còn lại
    for (int i = 1; i < totalFrames; ++i) {
        if (!cap.read(frame))
            break;
        writer.write(frame);
    }

    writer.release();

    // Đọc file thành buffer
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs)
        return false;

    std::vector<unsigned char> buffer(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );
    ifs.close();

    if (buffer.empty())
        return false;

    // Encode base64
    out_base64_video = base64_encode(buffer.data(), buffer.size());

    // Xoá file tạm (optional)
    std::remove(filename.c_str());

    return true;
}
