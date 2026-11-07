#pragma once

#include "common.hpp"
#include "continuation.hpp"

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/watch.html#ytmusicapi.YTMusic.get_watch_playlist
struct MusicNextRequest {
    std::string video_id;
    std::string playlist_id;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/watch.html#ytmusicapi.YTMusic.get_watch_playlist
struct MusicNextPage {
    std::vector<MusicItem> items;
    MusicEndpoint lyrics;
    MusicEndpoint related;
    std::optional<MusicContinuation> continuation;
};

void to_json(json& data, const MusicNextRequest& value);
void from_json(const json& data, MusicNextPage& value);
void to_json(json& data, const MusicNextPage& value);
