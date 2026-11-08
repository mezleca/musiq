#include "session.hpp"
#include "client.hpp"
#include "../http/http-client.hpp"
#include "../utils/log.hpp"

#include <boost/hash2/sha1.hpp>
#include <chrono>
#include <format>
#include <utility>
#include <algorithm>

static std::string_view trim(std::string_view value) {
    const auto start = value.find_first_not_of(" \t");
    return start == std::string_view::npos ? std::string_view{} : value.substr(start, value.find_last_not_of(" \t") - start + 1);
}

static std::string_view cookie_sapisid(std::string_view cookies) {
    // cookies are ';'-separated name=value pairs, SAPISID or __Secure-3PAPISID signs authenticated requests
    while (!cookies.empty()) {
        const auto end = cookies.find(';');
        const auto pair = cookies.substr(0, end);
        const auto separator = pair.find('=');

        if (separator != std::string_view::npos) {
            const auto name = trim(pair.substr(0, separator));
            const auto value = trim(pair.substr(separator + 1));
            if (!value.empty() && (name == "SAPISID" || name == "__Secure-3PAPISID")) {
                return value;
            }
        }

        if (end == std::string_view::npos) {
            break;
        }

        cookies.remove_prefix(end + 1);
    }

    return {};
}

// ref: https://github.com/SimoHypers/limusic/blob/d82372cdd3016e3c5e28ea8022596b7fefdf681a/crates/innertube/src/transport.rs#L769
static std::string sapisid_hash(std::string_view sapisid, int64_t timestamp) {
    const auto input = std::format("{} {} https://music.youtube.com", timestamp, sapisid);
    boost::hash2::sha1_160 sha1;
    sha1.update(input.data(), input.size());
    return std::format("SAPISIDHASH {}_{}", timestamp, boost::hash2::to_string(sha1.result()));
}

bool YoutubeMusicSession::has_session_cookie(std::string_view cookies) {
    // reject newline and nul bytes so the cookie header cannot be split
    return cookies.find_first_of("\r\n") == std::string_view::npos && cookies.find('\0') == std::string_view::npos &&
           !cookie_sapisid(cookies).empty();
}

std::optional<cpr::Header> YoutubeMusicSession::authentication_headers() const {
    if (m_sapisid.empty()) {
        return std::nullopt;
    }

    // the timestamp is part of the signature, so the header is rebuilt for every request
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return cpr::Header{
        {"Authorization", sapisid_hash(m_sapisid, std::chrono::duration_cast<std::chrono::seconds>(now).count())},
        {"Cookie", m_cookies}
    };
}

bool YoutubeMusicSession::authenticate(std::string cookies, std::string_view identity) {
    if (cookies.find_first_of("\r\n") != std::string_view::npos || cookies.find('\0') != std::string_view::npos) {
        return false;
    }

    YoutubeMusicSession session;
    session.m_sapisid = cookie_sapisid(cookies);
    if (session.m_sapisid.empty()) return false;

    session.m_cookies = std::move(cookies);

    auto response = YoutubeMusicClient(session).accounts_list();
    if (!response) {
        LOG_ERROR("[music] accounts_list failed: {} (status={})", response.error().message, response.error().status);
        return false;
    }

    session.m_identities = std::move(response->identities);

    if (session.m_identities.empty()) {
        LOG_ERROR("[music] no selectable YouTube channels returned");
        return false;
    }

    if (identity.empty() && session.m_identities.size() == 1) {
        identity = session.m_identities.front().data_sync_id;
    }

    if (!identity.empty() && !session.select_identity(identity)) return false;

    *this = std::move(session);
    return true;
}

bool YoutubeMusicSession::select_identity(std::string_view identity) {
    const auto selected = std::find_if(m_identities.begin(), m_identities.end(), [&](const MusicIdentity& item) {
        return item.data_sync_id == identity;
    });

    if (selected == m_identities.end()) {
        return false;
    }

    const auto response = YoutubeMusicClient(*this).account_menu(identity);
    if (!response) {
        LOG_ERROR("[music] account_menu failed: {} (status={})", response.error().message, response.error().status);
        return false;
    }

    if (response->data_sync_id != identity && (selected->handle.empty() || selected->handle != response->handle)) {
        LOG_ERROR("[music] selected channel was not confirmed");
        return false;
    }

    MusicUser user;
    user.name = response->name;
    user.avatar_url = response->avatar_url;

    if (!user.avatar_url.empty()) {
        auto avatar = HttpClient::instance().get(user.avatar_url);
        if (HttpClient::successful(avatar)) {
            user.avatar_data = std::move(avatar.text);
        }
    }

    m_user = std::move(user);
    m_identity = identity;
    m_data_sync_token = response->data_sync_token;
    return true;
}
