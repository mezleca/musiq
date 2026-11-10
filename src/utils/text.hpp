#pragma once

#include <string_view>

namespace utils::text {
    inline std::string_view trim(std::string_view value) {
        const auto start = value.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) return {};

        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(start, end - start + 1);
    }
} // namespace utils::text
