#pragma once

#include "../api/api.hpp"
#include "../utils/json.hpp"
#include "config.hpp"

#include <optional>
#include <string>
#include <string_view>

struct MusicUser {
    std::string name;
    std::string avatar_url;
    std::string avatar_data;
};

class YoutubeMusicSession {
public:
    static bool has_session_cookie(std::string_view cookies);
    bool authenticate(std::string cookies);

    std::optional<json> post(std::string_view endpoint, json body = json::object(), bool use_auth = true) const;

    const std::optional<MusicUser>& user() const {
        return m_user;
    }

private:
    Api m_api{std::string(music_config::API_URL)};
    std::string m_cookies;
    std::string m_sapisid;
    std::optional<MusicUser> m_user;
};
