#include "utils/path_utils.hpp"

#include <cstdlib>
#include <system_error>

namespace {
bool is_subpath(const std::filesystem::path& path, const std::filesystem::path& root) {
    auto path_it = path.begin();
    auto root_it = root.begin();
    for (; root_it != root.end(); ++root_it, ++path_it) {
        if (path_it == path.end() || *path_it != *root_it) {
            return false;
        }
    }
    return true;
}
} // namespace

std::filesystem::path get_default_file_root() {
    const char* env_root = std::getenv("SERVER_FILE_ROOT");
    if (env_root && *env_root) {
        return std::filesystem::path(env_root);
    }
    return std::filesystem::current_path();
}

bool resolve_safe_path(const std::filesystem::path& root,
                       const std::string& raw,
                       SafePathResult& out) {
    std::error_code ec;
    std::filesystem::path normalized_root = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        ec.clear();
        normalized_root = std::filesystem::absolute(root, ec);
    }
    normalized_root = normalized_root.lexically_normal();

    std::filesystem::path candidate(raw);
    if (candidate.is_relative()) {
        candidate = normalized_root / candidate;
    }
    std::filesystem::path normalized = std::filesystem::weakly_canonical(candidate, ec);
    if (ec) {
        ec.clear();
        normalized = std::filesystem::absolute(candidate, ec);
    }
    normalized = normalized.lexically_normal();

    out.root = normalized_root;
    out.resolved = normalized;

    if (!is_subpath(normalized, normalized_root)) {
        out.error = "path_not_allowed";
        return false;
    }

    out.error.clear();
    return true;
}

bool resolve_safe_path(const std::string& raw, SafePathResult& out) {
    return resolve_safe_path(get_default_file_root(), raw, out);
}
