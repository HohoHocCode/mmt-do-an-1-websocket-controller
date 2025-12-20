#pragma once

#include <filesystem>
#include <string>

struct SafePathResult {
    std::filesystem::path resolved;
    std::filesystem::path root;
    std::string error;
};

std::filesystem::path get_default_file_root();

bool resolve_safe_path(const std::filesystem::path& root,
                       const std::string& raw,
                       SafePathResult& out);

bool resolve_safe_path(const std::string& raw, SafePathResult& out);
