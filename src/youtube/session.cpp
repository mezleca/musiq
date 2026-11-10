#include "session.hpp"
#include "client.hpp"
#include "../utils/text.hpp"

#include <boost/hash2/sha1.hpp>
#include <chrono>
#include <format>
#include <utility>
#include <algorithm>

static std::string_view cookie_sapisid(std::string_view cookies) {
    // cookies are ';'-separated name=value pairs, SAPISID or __Secure-3PAPISID signs authenticated requests
    while (!cookies.empty()) {
        const auto end = cookies.find(';');
        const auto pair = cookies.substr(0, end);
        const auto separator = pair.find('=');

        if (separator != std::string_view::npos) {
            const auto name = utils::text::trim(pair.substr(0, separator));
            const auto value = utils::text::trim(pair.substr(separator + 1));
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

bool YoutubeMusicSession::restore(const json& data) {
    try {
        YoutubeMusicSession session;
        session.m_cookies = data.at("cookies").get<std::string>();
        if (!has_session_cookie(session.m_cookies)) return false;
        session.m_sapisid = cookie_sapisid(session.m_cookies);
        session.m_identity = data.at("identity").get<std::string>();
        session.m_data_sync_token = data.at("data_sync_token").get<std::string>();
        const auto& user = data.at("user");
        session.m_user = MusicUser{user.at("name").get<std::string>(), user.at("avatar_url").get<std::string>()};
        for (const auto& item : data.at("identities")) {
            session.m_identities.push_back(
                {item.at("name").get<std::string>(), item.at("handle").get<std::string>(),
                 item.at("data_sync_id").get<std::string>()}
            );
        }
        const bool selected = std::ranges::contains(session.m_identities, session.m_identity, &MusicIdentity::data_sync_id);
        if (session.m_identity.empty() || session.m_user->name.empty() || !selected) return false;
        *this = std::move(session);
        return true;
    } catch (const json::exception&) {
        return false;
    }
}

json YoutubeMusicSession::snapshot() const {
    json data{
        {"cookies", m_cookies}, {"identity", m_identity}, {"data_sync_token", m_data_sync_token}, {"identities", json::array()}
    };
    for (const auto& item : m_identities) {
        data["identities"].push_back({{"name", item.name}, {"handle", item.handle}, {"data_sync_id", item.data_sync_id}});
    }
    if (m_user) data["user"] = {{"name", m_user->name}, {"avatar_url", m_user->avatar_url}};
    return data;
}

YoutubeResult<void> YoutubeMusicSession::authenticate(std::string cookies, std::string_view identity) {
    if (!has_session_cookie(cookies)) return youtube_error(YoutubeErrorKind::Authentication, "invalid session cookie");
    YoutubeMusicSession session;
    session.m_sapisid = cookie_sapisid(cookies);
    session.m_cookies = std::move(cookies);
    auto result = session.load_identities();
    if (!result) return result;

    if (identity.empty() && session.m_identities.size() == 1) {
        identity = session.m_identities.front().data_sync_id;
    }

    if (!identity.empty()) {
        result = session.select_identity(identity);
        if (!result) return result;
    }

    *this = std::move(session);
    return {};
}

YoutubeResult<void> YoutubeMusicSession::load_identities() {
    auto response = YoutubeMusicClient(*this).accounts_list();
    if (!response) return std::unexpected(std::move(response.error()));
    if (response->identities.empty()) return youtube_error(YoutubeErrorKind::Api, "no selectable YouTube channels returned");
    m_identities = std::move(response->identities);
    return {};
}

YoutubeResult<void> YoutubeMusicSession::select_identity(std::string_view identity) {
    const auto selected = std::ranges::find(m_identities, identity, &MusicIdentity::data_sync_id);

    if (selected == m_identities.end()) {
        return youtube_error(YoutubeErrorKind::Authentication, "selected channel is unavailable");
    }

    auto response = YoutubeMusicClient(*this).account_menu(identity);
    if (!response) {
        return std::unexpected(std::move(response.error()));
    }

    if (response->data_sync_id != identity && (selected->handle.empty() || selected->handle != response->handle)) {
        return youtube_error(YoutubeErrorKind::Authentication, "selected channel was not confirmed");
    }

    m_user = MusicUser{std::move(response->name), std::move(response->avatar_url)};
    m_identity = identity;
    m_data_sync_token = std::move(response->data_sync_token);
    return {};
}
