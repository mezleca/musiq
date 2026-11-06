#include "client.hpp"
#include "session.hpp"
#include "config.hpp"
#include "../http/http-client.hpp"

#include <format>
#include <utility>

using namespace music_config;

static YoutubeResult<YoutubeResponse> read_response(const cpr::Response& response) {
    if (response.error.code != cpr::ErrorCode::OK) {
        return youtube_transport_error(response.status_code, static_cast<int>(response.error.code));
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        return youtube_error(YoutubeErrorKind::Http, "HTTP request failed", response.status_code);
    }

    auto data = json::parse(response.text, nullptr, false);
    if (data.is_discarded()) return youtube_error(YoutubeErrorKind::Parse, "invalid JSON response", response.status_code);
    if (data.contains("error")) return youtube_error(YoutubeErrorKind::Api, "YouTube rejected request", response.status_code);

    return YoutubeResponse{std::move(data), response.status_code};
}

YoutubeMusicClient::YoutubeMusicClient(const YoutubeMusicSession& session) : m_session(session) {}

YoutubeResult<MusicAccountMenu> YoutubeMusicClient::account_menu(std::string_view identity) const {
    return youtube_parse<MusicAccountMenu>(post("account/account_menu", json::object(), {.auth = MusicAuth::Required}, identity));
}

YoutubeResult<MusicAccountsList> YoutubeMusicClient::accounts_list() const {
    return youtube_parse<MusicAccountsList>(post("account/accounts_list", json::object(), {.auth = MusicAuth::Required}));
}

YoutubeResult<MusicVisitorData> YoutubeMusicClient::visitor_data() const {
    auto& http = HttpClient::instance();
    auto response = http.get(
        "https://music.youtube.com/sw.js_data", {{"User-Agent", std::string(client_profile(MusicClient::WebRemix).user_agent)}}
    );
    const auto start = response.text.find('[');

    // sw.js_data prefixes the json array with an anti-xssi marker
    if (start != std::string::npos) response.text.erase(0, start);
    return youtube_parse<MusicVisitorData>(read_response(response));
}

YoutubeResult<MusicPlayerData>
YoutubeMusicClient::player(const MusicPlayerRequest& request, const MusicRequestOptions& options) const {
    return youtube_parse<MusicPlayerData>(post("player", request, options));
}

const std::string& YoutubeMusicClient::identity() const {
    return m_session.identity();
}

MusicRequestOptions YoutubeMusicClient::request_options(MusicRequestOptions options) const {
    if (options.auth == MusicAuth::Optional) {
        options.auth = client_profile(options.client).login_supported && m_session.has_credentials() ? MusicAuth::Required
                                                                                                     : MusicAuth::Anonymous;
    }
    return options;
}

YoutubeResult<cpr::Header> YoutubeMusicClient::headers(const MusicRequestOptions& options) const {
    const auto& profile = client_profile(options.client);
    cpr::Header headers;
    if (options.auth == MusicAuth::Required) {
        if (!profile.login_supported) return youtube_error(YoutubeErrorKind::Authentication, "client does not support login");
        auto authentication = m_session.authentication_headers();
        if (!authentication) return youtube_error(YoutubeErrorKind::Authentication, "session required");
        headers = std::move(*authentication);
    }
    headers.insert(
        {{"Origin", "https://music.youtube.com"},
         {"X-Origin", "https://music.youtube.com"},
         {"Referer", std::string(HOME_URL)},
         {"X-YouTube-Client-Name", std::string(profile.id)},
         {"X-YouTube-Client-Version", std::string(profile.version)},
         {"X-Goog-Api-Format-Version", "1"},
         {"User-Agent", std::string(profile.user_agent)}}
    );
    if (!options.visitor_data.empty()) headers["X-Goog-Visitor-Id"] = options.visitor_data;
    return headers;
}

YoutubeResult<YoutubeResponse> YoutubeMusicClient::post(
    std::string_view endpoint, json body, const MusicRequestOptions& options, std::string_view identity,
    cpr::Parameters parameters
) const {
    const auto resolved = request_options(options);
    auto request_headers = headers(resolved);
    if (!request_headers) return std::unexpected(std::move(request_headers.error()));

    if (resolved.auth == MusicAuth::Anonymous) {
        identity = {};
    } else if (identity.empty()) {
        identity = m_session.identity();
    }
    body["context"] = MusicRequestContext{resolved.client, resolved.visitor_data, identity};
    parameters.Add({"prettyPrint", "false"});

    auto& http = HttpClient::instance();
    return read_response(
        http.post_json(std::format("{}/{}", API_URL, endpoint), body, std::move(*request_headers), std::move(parameters))
    );
}
