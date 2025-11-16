#include "modules/screen/ScreenCapturer.hpp"
#include <spdlog/spdlog.h>

#ifdef __linux__

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <sys/shm.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>   // cv::cvtColor, COLOR_BGRA2BGR

cv::Mat ScreenCapturer::captureScreenLinux() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        spdlog::error("[Linux] Cannot open X11 display!");
        return captureDummy();
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    XWindowAttributes gwa;
    XGetWindowAttributes(display, root, &gwa);

    int width = gwa.width;
    int height = gwa.height;

    // ---- XShm setup ----
    XShmSegmentInfo shminfo{};
    XImage* img = XShmCreateImage(
        display,
        DefaultVisual(display, screen),
        DefaultDepth(display, screen),
        ZPixmap,
        nullptr,
        &shminfo,
        width,
        height
    );

    if (!img) {
        spdlog::error("[Linux] XShmCreateImage failed");
        XCloseDisplay(display);
        return captureDummy();
    }

    shminfo.shmid = shmget(IPC_PRIVATE, img->bytes_per_line * img->height, IPC_CREAT | 0777);
    if (shminfo.shmid < 0) {
        spdlog::error("[Linux] shmget failed");
        XDestroyImage(img);
        XCloseDisplay(display);
        return captureDummy();
    }

    shminfo.shmaddr = (char*)shmat(shminfo.shmid, nullptr, 0);
    img->data = shminfo.shmaddr;
    shminfo.readOnly = False;

    if (!XShmAttach(display, &shminfo)) {
        spdlog::error("[Linux] XShmAttach failed");
        shmdt(shminfo.shmaddr);
        shmctl(shminfo.shmid, IPC_RMID, 0);
        XDestroyImage(img);
        XCloseDisplay(display);
        return captureDummy();
    }

    if (!XShmGetImage(display, root, img, 0, 0, AllPlanes)) {
        spdlog::error("[Linux] XShmGetImage failed");
        XShmDetach(display, &shminfo);
        shmdt(shminfo.shmaddr);
        shmctl(shminfo.shmid, IPC_RMID, 0);
        XDestroyImage(img);
        XCloseDisplay(display);
        return captureDummy();
    }

    XSync(display, False);

    // ---- Convert XImage (BGRA) → OpenCV BGR ----
    cv::Mat rgba(height, width, CV_8UC4, (uchar*)img->data);
    cv::Mat bgr;
    cv::cvtColor(rgba, bgr, cv::COLOR_BGRA2BGR);

    // ---- Cleanup ----
    XShmDetach(display, &shminfo);
    shmdt(shminfo.shmaddr);
    shmctl(shminfo.shmid, IPC_RMID, 0);
    XDestroyImage(img);
    XCloseDisplay(display);

    spdlog::info("[Linux] Screenshot captured: {}x{}", width, height);
    return bgr;
}

#endif // __linux__
