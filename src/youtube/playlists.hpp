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

private:
    const YoutubeMusicClient& m_client;
};
