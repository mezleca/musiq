#pragma once

#include <cpr/cprtypes.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct MusicIdentity {
    std::string name;
    std::string handle;
    std::string data_sync_id;
};

struct MusicUser {
    std::string name;
    std::string avatar_url;
    std::string avatar_data;
    std::string channel_url;
};

class YoutubeMusicSession {
public:
    static bool has_session_cookie(std::string_view cookies);
    // user() remains empty until a channel is selected when several identities are available
    bool authenticate(std::string cookies, std::string_view identity = {});
    bool select_identity(std::string_view identity);

    std::optional<cpr::Header> authentication_headers() const;

    const std::string& identity() const {
        return m_identity;
    }

    const std::vector<MusicIdentity>& identities() const {
        return m_identities;
    }

    const std::optional<MusicUser>& user() const {
        return m_user;
    }

private:
    std::string m_cookies;
    std::string m_sapisid;
    std::string m_identity;
    std::vector<MusicIdentity> m_identities;
    std::optional<MusicUser> m_user;
};
