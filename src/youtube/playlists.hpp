#pragma once

#include "error.hpp"
#include "schema/playlist.hpp"

class YoutubeMusicClient;

class YoutubeMusicPlaylists {
public:
    explicit YoutubeMusicPlaylists(const YoutubeMusicClient& client);

    // reads every saved playlist page for the authenticated channel
    YoutubeResult<std::vector<MusicItem>> library(const MusicRequestOptions& options = {}) const;

    // returns details and one track page, further pages are fetched explicitly through more()
    YoutubeResult<MusicPlaylist> get(std::string_view playlist_id, const MusicRequestOptions& options = {}) const;
    YoutubeResult<MusicPlaylistPage> more(const MusicContinuation& continuation) const;

    // writes require an authenticated session and return the server status
    YoutubeResult<MusicPlaylistCreated>
    create(const MusicPlaylistCreateRequest& request, const MusicRequestOptions& options = {}) const;
    YoutubeResult<MusicPlaylistEditResult>
    add(const MusicPlaylistAddRequest& request, const MusicRequestOptions& options = {}) const;
    YoutubeResult<MusicPlaylistEditResult>
    remove(const MusicPlaylistRemoveRequest& request, const MusicRequestOptions& options = {}) const;
    YoutubeResult<MusicPlaylistEditResult>
    update(const MusicPlaylistUpdateRequest& request, const MusicRequestOptions& options = {}) const;

private:
    const YoutubeMusicClient& m_client;
};
