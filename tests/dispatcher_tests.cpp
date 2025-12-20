#include "doctest/doctest.h"
#include "core/dispatcher.hpp"
#include "utils/json.hpp"
#include "utils/limits.hpp"

TEST_CASE("dispatcher handles invalid JSON safely") {
    Dispatcher dispatcher;
    std::string response = dispatcher.handle("{invalid_json");
    Json parsed = Json::parse(response);

    CHECK(parsed["status"] == "error");
    CHECK(parsed["error"] == "invalid_json");
}

TEST_CASE("dispatcher rejects oversized messages") {
    Dispatcher dispatcher;
    std::string oversized(limits::kMaxMessageBytes + 1, 'a');
    std::string response = dispatcher.handle(oversized);
    Json parsed = Json::parse(response);

    CHECK(parsed["status"] == "error");
    CHECK(parsed["error"] == "message_too_large");
}
