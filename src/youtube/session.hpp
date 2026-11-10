#pragma once

#include "schema/account.hpp"
#include "error.hpp"

#include <cpr/cprtypes.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct MusicUser {
    std::string name;
    std::string avatar_url;
};

class YoutubeMusicSession {
public:
    static bool has_session_cookie(std::string_view cookies);

    bool restore(const json& data);
    json snapshot() const;
    YoutubeResult<void> authenticate(std::string cookies, std::string_view identity = {});
    YoutubeResult<void> select_identity(std::string_view identity);
    YoutubeResult<void> load_identities();

    std::optional<cpr::Header> authentication_headers() const;

    bool has_credentials() const {
        return !m_sapisid.empty();
    }

    const std::string& identity() const {
        return m_identity;
    }
    const std::string& data_sync_token() const {
        return m_data_sync_token;
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
    std::string m_data_sync_token;
    std::vector<MusicIdentity> m_identities;
    std::optional<MusicUser> m_user;
};
