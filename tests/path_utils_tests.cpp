#include "doctest/doctest.h"
#include "utils/path_utils.hpp"

#include <filesystem>

TEST_CASE("resolve_safe_path enforces root and blocks traversal") {
    std::filesystem::path root = std::filesystem::temp_directory_path() / "mmt_safe_root";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "subdir");

    SafePathResult result;
    CHECK(resolve_safe_path(root, "subdir/file.txt", result));
    CHECK(result.resolved.lexically_normal().string().find(root.lexically_normal().string()) == 0);

    SafePathResult traversal;
    CHECK_FALSE(resolve_safe_path(root, "../outside.txt", traversal));
    CHECK(traversal.error == "path_not_allowed");

    SafePathResult absolute;
    std::filesystem::path outside = std::filesystem::temp_directory_path() / "outside.txt";
    CHECK_FALSE(resolve_safe_path(root, outside.string(), absolute));
    CHECK(absolute.error == "path_not_allowed");
}
