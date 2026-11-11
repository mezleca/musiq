#include "playlists.hpp"
#include "client.hpp"
#include "catalog.hpp"
#include "../utils/log.hpp"

#include <utility>
#include <unordered_set>

YoutubeMusicPlaylists::YoutubeMusicPlaylists(const YoutubeMusicClient& client) : m_client(client) {}

YoutubeResult<std::vector<MusicItem>> YoutubeMusicPlaylists::library(const MusicRequestOptions& options) const {
    auto authenticated = options;
    authenticated.auth = MusicAuth::Required;
    const YoutubeMusicCatalog catalog(m_client);
    auto page = catalog.browse({.browse_id = "FEmusic_liked_playlists", .params = {}}, authenticated);
    std::vector<MusicItem> playlists;
    std::unordered_set<std::string> ids;
    std::unordered_set<std::string> tokens;

    while (page) {
        auto continuation = page->continuation;
        for (auto& shelf : page->shelves) {
            for (auto& item : shelf.items) {
                const auto& id = item.endpoint.browse_id;
                if (id.starts_with("VL") && ids.insert(id).second) playlists.push_back(std::move(item));
            }
            if (!continuation && shelf.continuation) continuation = shelf.continuation;
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
