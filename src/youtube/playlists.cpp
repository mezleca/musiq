#include "playlists.hpp"
#include "client.hpp"
#include "catalog.hpp"
#include "../utils/log.hpp"

#include <utility>
#include <unordered_set>

YoutubeMusicPlaylists::YoutubeMusicPlaylists(const YoutubeMusicClient& client) : m_client(client) {}

static MusicRequestOptions write_options(MusicRequestOptions options) {
    options.auth = MusicAuth::Required;
    return options;
}

static bool succeeded(const MusicPlaylistEditResult& result) {
    return result.status.find("SUCCEEDED") != std::string::npos;
}

YoutubeResult<std::vector<MusicItem>> YoutubeMusicPlaylists::library(const MusicRequestOptions& options) const {
    auto authenticated = options;
    authenticated.auth = MusicAuth::Required;
    const YoutubeMusicCatalog catalog(m_client);
    auto page = catalog.browse({.browse_id = "FEmusic_liked_playlists", .params = {}}, authenticated);
    std::vector<MusicItem> playlists;
    std::unordered_set<std::string> ids;
    std::unordered_set<std::string> tokens;

    while (page) {
        auto continuation = std::move(page->continuation);
        for (auto& shelf : page->shelves) {
            for (auto& item : shelf.items) {
                const auto& id = item.endpoint.browse_id;
                if (id.starts_with("VL") && ids.insert(id).second) playlists.push_back(std::move(item));
            }
            if (!continuation && shelf.continuation) continuation = std::move(shelf.continuation);
        }

        if (!continuation) return playlists;
        if (!tokens.insert(continuation->token).second) {
            return youtube_error(YoutubeErrorKind::Api, "repeated library playlist continuation");
        }
        page = catalog.more(*continuation);
    }

    return std::unexpected(std::move(page.error()));
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

    auto result = youtube_parse<MusicPlaylistEditResult>(m_client.post("browse/edit_playlist", request, write_options(options)));
    if (!result) return std::unexpected(std::move(result.error()));
    if (!succeeded(*result)) {
        return youtube_error(YoutubeErrorKind::Api, result->status.empty() ? "playlist add failed" : result->status);
    }

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

    auto result = youtube_parse<MusicPlaylistEditResult>(m_client.post("browse/edit_playlist", request, write_options(options)));
    if (!result) return std::unexpected(std::move(result.error()));
    if (!succeeded(*result)) {
        return youtube_error(YoutubeErrorKind::Api, result->status.empty() ? "playlist removal failed" : result->status);
    }

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
    auto result = youtube_parse<MusicPlaylistEditResult>(m_client.post("browse/edit_playlist", update, write_options(options)));
    if (!result) return std::unexpected(std::move(result.error()));
    if (!succeeded(*result)) {
        return youtube_error(YoutubeErrorKind::Api, result->status.empty() ? "playlist update failed" : result->status);
    }

    LOG_INFO("[playlist] updated {}", request.playlist_id);
    return result;
}
