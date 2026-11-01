#include "ytb-music-session.hpp"
#include "../utils/log.hpp"

#include <boost/hash2/sha1.hpp>
#include <chrono>
#include <format>
#include <utility>

using namespace music_config;

static std::string_view trim(std::string_view value) {
    const auto start = value.find_first_not_of(" \t");
    return start == std::string_view::npos ? std::string_view{} : value.substr(start, value.find_last_not_of(" \t") - start + 1);
}

static std::string_view cookie_sapisid(std::string_view cookies) {
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
    const auto digest = sha1.result();

    std::string hash;
    hash.reserve(digest.size() * 2);
    for (const auto byte : digest) {
        hash += std::format("{:02x}", byte);
    }
    return std::format("SAPISIDHASH {}_{}", timestamp, hash);
}

bool YoutubeMusicSession::has_session_cookie(std::string_view cookies) {
    return cookies.find_first_of("\r\n") == std::string_view::npos && cookies.find('\0') == std::string_view::npos &&
           !cookie_sapisid(cookies).empty();
}

std::optional<json> YoutubeMusicSession::post(std::string_view endpoint, json body, bool use_auth) const {
    if (endpoint.empty() || !body.is_object() || (use_auth && m_sapisid.empty())) {
        LOG_ERROR("[api] invalid request or missing youtube session");
        return std::nullopt;
    }

    body["context"]["client"] = {{"clientName", "WEB_REMIX"}, {"clientVersion", CLIENT_VERSION}, {"hl", "en"}, {"gl", "US"}};
    cpr::Header headers{
        {"Origin", "https://music.youtube.com"},
        {"X-Origin", "https://music.youtube.com"},
        {"Referer", std::string(HOME_URL)},
        {"X-YouTube-Client-Name", "67"},
        {"X-YouTube-Client-Version", std::string(CLIENT_VERSION)},
        {"X-Goog-Api-Format-Version", "1"},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:140.0) Gecko/20100101 Firefox/140.0"}
    };

    if (use_auth) {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        headers["Authorization"] = sapisid_hash(m_sapisid, std::chrono::duration_cast<std::chrono::seconds>(now).count());
        headers["Cookie"] = m_cookies;
    }

    return m_api.post(endpoint, body, std::move(headers));
}

static const json* find_account_header(const json& node) {
    if (node.is_object() && node.contains("activeAccountHeaderRenderer")) {
        return &node["activeAccountHeaderRenderer"];
    }

    if (node.is_structured()) {
        for (const auto& child : node) {
            if (const auto* header = find_account_header(child)) {
                return header;
            }
        }
    }

    return nullptr;
}

bool YoutubeMusicSession::authenticate(std::string cookies) {
    if (!has_session_cookie(cookies)) {
        return false;
    }

    YoutubeMusicSession session;
    session.m_sapisid = cookie_sapisid(cookies);
    session.m_cookies = std::move(cookies);

    const auto response = session.post("account/account_menu");
    if (!response) {
        return false;
    }

    try {
        const auto* header = find_account_header(*response);
        if (!header) {
            return false;
        }

        MusicUser user;
        const auto& name = header->at("accountName");
        if (name.contains("simpleText")) {
            user.name = name.at("simpleText").get<std::string>();
        } else {
            for (const auto& run : name.at("runs")) {
                user.name += run.at("text").get<std::string>();
            }
        }

        if (user.name.empty()) {
            return false;
        }

        if (header->contains("accountPhoto")) {
            const auto& thumbnails = header->at("accountPhoto").at("thumbnails");
            if (!thumbnails.empty()) {
                user.avatar_url = thumbnails.back().at("url").get<std::string>();
            }
        }

        if (!user.avatar_url.empty()) {
            user.avatar_data = Api::download(user.avatar_url).value_or("");
        }

        session.m_user = std::move(user);
    } catch (const json::exception&) {
        LOG_ERROR("[music] invalid account menu response");
        return false;
    }

    *this = std::move(session);
    return true;
}
