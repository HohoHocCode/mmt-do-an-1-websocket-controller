#pragma once
#include "utils/json.hpp"
#include <string>

class ProcessManager {
public:
    Json list_processes();
    Json kill_process(int pid);
    Json start_process(const std::string& path);
};
