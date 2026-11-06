#pragma once

#include "error.hpp"
#include "schema/queue.hpp"

class YoutubeMusicClient;

class YoutubeMusicQueue {
public:
    explicit YoutubeMusicQueue(const YoutubeMusicClient& client);
    YoutubeResult<MusicNextPage> next(const MusicNextRequest& request, const MusicRequestOptions& options = {}) const;
    YoutubeResult<MusicNextPage> more(const MusicContinuation& continuation) const;

private:
    YoutubeResult<MusicNextPage> read(json body, const MusicRequestOptions& options) const;

    const YoutubeMusicClient& m_client;
};
