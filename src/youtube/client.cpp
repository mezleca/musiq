#include "client.hpp"
#include "session.hpp"
#include "config.hpp"
#include "../http/http-client.hpp"
#include "../utils/log.hpp"

#include <format>
#include <utility>

using namespace music_config;

YoutubeMusicClient::YoutubeMusicClient(const YoutubeMusicSession& session) : m_session(session) {}

std::optional<json> YoutubeMusicClient::account_menu() const {
    return post("account/account_menu");
}

std::optional<json> YoutubeMusicClient::post(std::string_view endpoint, json body) const {
    auto headers = m_session.authentication_headers();
    if (!headers) {
        return std::nullopt;
    }

    headers->insert(
        {{"Origin", "https://music.youtube.com"},
         {"X-Origin", "https://music.youtube.com"},
         {"Referer", std::string(HOME_URL)},
         {"X-YouTube-Client-Name", "67"},
         {"X-YouTube-Client-Version", std::string(CLIENT_VERSION)},
         {"X-Goog-Api-Format-Version", "1"},
         {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:140.0) Gecko/20100101 Firefox/140.0"}}
    );

    body["context"]["client"] = {{"clientName", "WEB_REMIX"}, {"clientVersion", CLIENT_VERSION}, {"hl", "en"}, {"gl", "US"}};

    auto& http = HttpClient::instance();
    const auto response = http.post_json(std::format("{}/{}", API_URL, endpoint), body, std::move(*headers));
    auto result = HttpClient::parse_json(response);
    if (!result) {
        LOG_ERROR("[music] request failed: status={} transport={}", response.status_code, static_cast<int>(response.error.code));
    }

    return result;
}
