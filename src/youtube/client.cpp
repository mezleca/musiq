#include "client.hpp"
#include "session.hpp"
#include "../constants.hpp"
#include "../http/http-client.hpp"

#include <format>
#include <fstream>
#include <iterator>
#include <cctype>
#include <algorithm>
#include <utility>

static YoutubeResult<YoutubeResponse> read_response(const cpr::Response& response) {
    if (response.error.code != cpr::ErrorCode::OK) {
        return youtube_transport_error(response.status_code, static_cast<int>(response.error.code));
    }
    if (!http::successful(response)) {
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
    auto response = http::get(
        "https://music.youtube.com/sw.js_data",
        {{"User-Agent", std::string(music_client_profile(MusicClient::WebRemix).user_agent)}}
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
        options.auth = music_client_profile(options.client).login_supported && m_session.has_credentials() ? MusicAuth::Required
                                                                                                           : MusicAuth::Anonymous;
    }
    return options;
}

YoutubeResult<cpr::Header> YoutubeMusicClient::headers(const MusicRequestOptions& options) const {
    const auto& profile = music_client_profile(options.client);
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
         {"Referer", std::string(MUSIC_HOME_URL)},
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

    return read_response(
        http::post_json(std::format("{}/{}", MUSIC_API_URL, endpoint), body, std::move(*request_headers), std::move(parameters))
    );
}

YoutubeResult<std::string> YoutubeMusicClient::upload_playlist_thumbnail(const std::filesystem::path& path) const {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (extension != ".jpg" && extension != ".jpeg" && extension != ".png") {
        return youtube_error(YoutubeErrorKind::Api, "playlist covers must be JPEG or PNG");
    }

    constexpr std::uintmax_t max_size = 8 * 1024 * 1024;
    std::error_code file_error;
    const auto size = std::filesystem::file_size(path, file_error);
    if (file_error || size == 0) return youtube_error(YoutubeErrorKind::Api, "cannot read playlist cover");
    if (size > max_size) return youtube_error(YoutubeErrorKind::Api, "playlist cover exceeds 8 MiB");

    std::ifstream file(path, std::ios::binary);
    if (!file) return youtube_error(YoutubeErrorKind::Api, "cannot open playlist cover");
    std::string bytes(std::istreambuf_iterator<char>(file), {});
    if (bytes.empty()) return youtube_error(YoutubeErrorKind::Api, "playlist cover is empty");

    const auto options = request_options({.auth = MusicAuth::Required});
    auto request_headers = headers(options);
    if (!request_headers) return std::unexpected(std::move(request_headers.error()));

    constexpr std::string_view endpoint = "https://music.youtube.com/playlist_image_upload/playlist_custom_thumbnail";
    auto handshake_headers = *request_headers;
    handshake_headers["Content-Type"] = "application/x-www-form-urlencoded;charset=utf-8";
    handshake_headers["X-Goog-Upload-Command"] = "start";
    handshake_headers["X-Goog-Upload-Header-Content-Length"] = std::to_string(bytes.size());
    handshake_headers["X-Goog-Upload-Protocol"] = "resumable";
    auto handshake = http::request({
        .method = HttpMethod::Post,
        .url = std::string(endpoint),
        .headers = std::move(handshake_headers),
        .parameters = {},
        .body = {},
    });
    if (!http::successful(handshake)) {
        return youtube_error(YoutubeErrorKind::Http, "playlist cover upload handshake failed", handshake.status_code);
    }

    const auto upload_id = handshake.header.find("x-guploader-uploadid");
    if (upload_id == handshake.header.end() || upload_id->second.empty()) {
        return youtube_error(YoutubeErrorKind::Api, "playlist cover upload id missing");
    }

    auto upload_headers = *request_headers;
    if (extension == ".png") {
        upload_headers["Content-Type"] = "image/png";
    } else {
        upload_headers["Content-Type"] = "image/jpeg";
    }
    upload_headers["X-Goog-Upload-Command"] = "upload, finalize";
    upload_headers["X-Goog-Upload-Offset"] = "0";
    auto uploaded = http::request({
        .method = HttpMethod::Post,
        .url = std::string(endpoint),
        .headers = std::move(upload_headers),
        .parameters = {{"upload_id", upload_id->second}, {"upload_protocol", "resumable"}},
        .body = std::move(bytes),
    });
    if (!http::successful(uploaded)) {
        return youtube_error(YoutubeErrorKind::Http, "playlist cover upload failed", uploaded.status_code);
    }

    const auto response = json::parse(uploaded.text, nullptr, false);
    if (response.is_discarded()) return youtube_error(YoutubeErrorKind::Parse, "invalid playlist cover upload response");
    auto blob = response.value("playlistScottyEncryptedBlobId", response.value("encryptedBlobId", std::string{}));
    if (blob.empty()) return youtube_error(YoutubeErrorKind::Api, "playlist cover upload returned no blob id");
    return blob;
}
