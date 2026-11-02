#pragma once

#include <string_view>

namespace music_config {
    inline constexpr std::string_view HOME_URL = "https://music.youtube.com/";
    inline constexpr std::string_view API_URL = "https://music.youtube.com/youtubei/v1";
    inline constexpr std::string_view LOGIN_URL =
        "https://accounts.google.com/ServiceLogin?service=youtube&continue=https://music.youtube.com/";
    inline constexpr std::string_view CLIENT_VERSION = "1.20260213.01.00";
} // namespace music_config
