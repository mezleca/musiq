#include "playlists.hpp"
#include "client.hpp"
#include "../utils/log.hpp"

#include <utility>

YoutubeMusicPlaylists::YoutubeMusicPlaylists(const YoutubeMusicClient& client) : m_client(client) {}

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
