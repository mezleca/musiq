#pragma once

#include <filesystem>

namespace utils::path {
    std::filesystem::path from_resources(const std::filesystem::path& relative_path);
} // namespace utils::path
