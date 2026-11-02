#pragma once

#include <cpr/cprtypes.h>

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

    std::optional<cpr::Header> authentication_headers() const;

    const std::optional<MusicUser>& user() const {
        return m_user;
    }

private:
    std::string m_cookies;
    std::string m_sapisid;
    std::optional<MusicUser> m_user;
};
