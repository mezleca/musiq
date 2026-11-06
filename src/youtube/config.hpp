#pragma once

#include <string_view>

enum class MusicClient;

namespace music_config {
    inline constexpr std::string_view HOME_URL = "https://music.youtube.com/";
    inline constexpr std::string_view API_URL = "https://music.youtube.com/youtubei/v1";
    inline constexpr std::string_view LOGIN_URL =
        "https://accounts.google.com/ServiceLogin?service=youtube&continue=https://music.youtube.com/";
    inline constexpr std::string_view CLIENT_VERSION = "1.20260213.01.00";

    struct MusicClientProfile {
        std::string_view name;
        std::string_view id;
        std::string_view version;
        std::string_view user_agent;
        bool login_supported;
        std::string_view os_name;
        std::string_view os_version;
        std::string_view device_make;
        std::string_view device_model;
    };

    const MusicClientProfile& client_profile(MusicClient client);
} // namespace music_config
