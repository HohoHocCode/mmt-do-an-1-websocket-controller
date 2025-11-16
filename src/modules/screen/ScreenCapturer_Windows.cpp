#if defined(_WIN32)

#include "modules/screen/ScreenCapturer.hpp"
#include <windows.h>
#include <spdlog/spdlog.h>

// Helper cho Windows – khớp với forward declare trong common
cv::Mat captureScreen_Windows() {
    // Lấy kích thước màn hình
    int screenX = GetSystemMetrics(SM_CXSCREEN);
    int screenY = GetSystemMetrics(SM_CYSCREEN);

    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC) {
        spdlog::error("[ScreenCapturer][Win] GetDC(NULL) failed");
        return cv::Mat();
    }

    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    if (!hMemoryDC) {
        spdlog::error("[ScreenCapturer][Win] CreateCompatibleDC failed");
        ReleaseDC(NULL, hScreenDC);
        return cv::Mat();
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenX, screenY);
    if (!hBitmap) {
        spdlog::error("[ScreenCapturer][Win] CreateCompatibleBitmap failed");
        DeleteDC(hMemoryDC);
        ReleaseDC(NULL, hScreenDC);
        return cv::Mat();
    }

    SelectObject(hMemoryDC, hBitmap);

    if (!BitBlt(hMemoryDC, 0, 0, screenX, screenY, hScreenDC, 0, 0, SRCCOPY)) {
        spdlog::error("[ScreenCapturer][Win] BitBlt failed");
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(NULL, hScreenDC);
        return cv::Mat();
    }

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = screenX;
    bi.biHeight = -screenY;   // âm để ảnh không bị lật ngược
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;

    cv::Mat img(screenY, screenX, CV_8UC3);
    if (!GetDIBits(hMemoryDC, hBitmap, 0, screenY, img.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS)) {
        spdlog::error("[ScreenCapturer][Win] GetDIBits failed");
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(NULL, hScreenDC);
        return cv::Mat();
    }

    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    spdlog::info("[ScreenCapturer][Win] Screen captured {}x{}", screenX, screenY);
    return img;
}

#endif // _WIN32
