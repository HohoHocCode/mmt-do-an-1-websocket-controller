#pragma once
#include "../external/nlohmann/json.hpp"

#include <string>

using Json = nlohmann::json;

struct JsonParseResult {
    bool ok = false;
    Json value;
    std::string error = "invalid_json";
};

inline JsonParseResult parse_json_safe(const std::string& input) {
    JsonParseResult result;
    Json parsed = Json::parse(input, nullptr, false);
    if (parsed.is_discarded()) {
        return result;
    }
    result.ok = true;
    result.value = std::move(parsed);
    result.error.clear();
    return result;
}
