#pragma once
#include <string>
#include "../utils/json.hpp"

struct Command {
    std::string name;
    Json        payload;
};
