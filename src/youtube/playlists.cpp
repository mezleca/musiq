#include "playlists.hpp"
#include "client.hpp"
#include "catalog.hpp"
#include "../utils/log.hpp"

#include <utility>

YoutubeMusicPlaylists::YoutubeMusicPlaylists(const YoutubeMusicClient& client) : m_client(client) {}

static MusicRequestOptions write_options(MusicRequestOptions options) {
    options.auth = MusicAuth::Required;
    return options;
}

static YoutubeResult<MusicPlaylistEditResult>
edit_playlist(const YoutubeMusicClient& client, json request, const MusicRequestOptions& options, const char* failure) {
    auto result =
        youtube_parse<MusicPlaylistEditResult>(client.post("browse/edit_playlist", std::move(request), write_options(options)));
    if (result && result->status.find("SUCCEEDED") == std::string::npos) {
        return youtube_error(YoutubeErrorKind::Api, result->status.empty() ? failure : result->status);
    }
    return result;
}

YoutubeResult<MusicPage> YoutubeMusicPlaylists::library(const MusicRequestOptions& options) const {
    return YoutubeMusicCatalog(m_client).browse({.browse_id = "FEmusic_liked_playlists", .params = {}}, write_options(options));
}

YoutubeResult<MusicPlaylist> YoutubeMusicPlaylists::get(std::string_view playlist_id, const MusicRequestOptions& options) const {
    if (playlist_id.empty()) return youtube_error(YoutubeErrorKind::Api, "playlist id required");

    LOG_INFO("[playlist] loading {}", playlist_id);
    const auto resolved = m_client.request_options(options);
    auto result = youtube_parse<MusicPlaylist>(m_client.post("browse", MusicPlaylistRequest{std::string(playlist_id)}, resolved));
    if (result) {
        if (result->id.empty()) result->id = playlist_id.starts_with("VL") ? playlist_id.substr(2) : playlist_id;
        if (result->continuation) result->continuation->set_scope(MusicRoute::Playlist, resolved, m_client.identity());
        LOG_INFO("[playlist] loaded {} (tracks={})", result->id, result->tracks.size());
    } else {
        LOG_ERROR("[playlist] load failed: {} (status={})", result.error().message, result.error().status);
    }

    return result;
}

YoutubeResult<MusicPlaylistPage> YoutubeMusicPlaylists::more(const MusicContinuation& continuation) const {
    if (continuation.route != MusicRoute::Playlist) return youtube_error(YoutubeErrorKind::Api, "not a playlist continuation");
    if (!continuation.owned_by(m_client.identity())) {
        return youtube_error(YoutubeErrorKind::Authentication, "continuation belongs to another channel");
    }

    const auto options = continuation.request_options();
    auto response = continuation.in_body
                        ? m_client.post("browse", {{"continuation", continuation.token}}, options)
                        : m_client.post(
                              "browse", json::object(), options, {},
                              {{"ctoken", continuation.token}, {"continuation", continuation.token}, {"type", "next"}}
                          );
    auto result = youtube_parse<MusicPlaylistPage>(std::move(response));
    if (result && result->continuation) result->continuation->set_scope(MusicRoute::Playlist, options, m_client.identity());
    if (!result) LOG_ERROR("[playlist] pagination failed: {} (status={})", result.error().message, result.error().status);

    return result;
}

YoutubeResult<MusicPlaylistCreated>
YoutubeMusicPlaylists::create(const MusicPlaylistCreateRequest& request, const MusicRequestOptions& options) const {
    if (request.title.empty()) return youtube_error(YoutubeErrorKind::Api, "playlist title required");
    if (request.title.find_first_of("<>") != std::string::npos) {
        return youtube_error(YoutubeErrorKind::Api, "playlist title contains invalid characters");
    }

    auto result = youtube_parse<MusicPlaylistCreated>(m_client.post("playlist/create", request, write_options(options)));
    if (!result) return std::unexpected(std::move(result.error()));
    if (result->id.empty()) {
        return youtube_error(YoutubeErrorKind::Api, result->status.empty() ? "playlist creation failed" : result->status);
    }

    LOG_INFO("[playlist] created {}", result->id);
    return result;
}

YoutubeResult<MusicPlaylistEditResult>
YoutubeMusicPlaylists::add(const MusicPlaylistAddRequest& request, const MusicRequestOptions& options) const {
    if (request.playlist_id.empty()) return youtube_error(YoutubeErrorKind::Api, "playlist id required");
    if (request.video_ids.empty()) return youtube_error(YoutubeErrorKind::Api, "no videos to add");

    auto result = edit_playlist(m_client, request, options, "playlist add failed");
    if (!result) return result;

    LOG_INFO("[playlist] added {} track(s)", request.video_ids.size());
    return result;
}

YoutubeResult<MusicPlaylistEditResult>
YoutubeMusicPlaylists::remove(const MusicPlaylistRemoveRequest& request, const MusicRequestOptions& options) const {
    if (request.playlist_id.empty()) return youtube_error(YoutubeErrorKind::Api, "playlist id required");
    if (request.entries.empty()) return youtube_error(YoutubeErrorKind::Api, "no playlist entries to remove");

    for (const auto& entry : request.entries) {
        if (entry.video_id.empty() || entry.set_video_id.empty()) {
            return youtube_error(YoutubeErrorKind::Api, "removal requires video and set video ids");
        }
    }

    auto result = edit_playlist(m_client, request, options, "playlist removal failed");
    if (!result) return result;

    LOG_INFO("[playlist] removed {} track(s)", request.entries.size());
    return result;
}

YoutubeResult<MusicPlaylistEditResult>
YoutubeMusicPlaylists::update(const MusicPlaylistUpdateRequest& request, const MusicRequestOptions& options) const {
    if (request.playlist_id.empty()) return youtube_error(YoutubeErrorKind::Api, "playlist id required");
    if (!request.title && !request.description && !request.privacy && !request.thumbnail_path && !request.thumbnail_blob_id) {
        return youtube_error(YoutubeErrorKind::Api, "no playlist changes");
    }
    if (request.title && request.title->empty()) return youtube_error(YoutubeErrorKind::Api, "playlist title required");

    auto update = request;
    if (update.thumbnail_path) {
        auto uploaded = m_client.upload_playlist_thumbnail(*update.thumbnail_path);
        if (!uploaded) return std::unexpected(std::move(uploaded.error()));
        update.thumbnail_blob_id = std::move(*uploaded);
        update.thumbnail_path.reset();
    }
    auto result = edit_playlist(m_client, update, options, "playlist update failed");
    if (!result) return result;

    LOG_INFO("[playlist] updated {}", request.playlist_id);
    return result;
}
