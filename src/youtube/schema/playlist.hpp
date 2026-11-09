#pragma once

#include "common.hpp"
#include "continuation.hpp"

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/playlists.html#ytmusicapi.YTMusic.get_playlist
struct MusicPlaylistRequest {
    std::string playlist_id;
};

struct MusicPlaylistPage {
    std::vector<MusicItem> tracks;
    std::optional<MusicContinuation> continuation;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/playlists.html#ytmusicapi.YTMusic.get_playlist
struct MusicPlaylist {
    std::string id;
    std::string title;
    std::string description;
    MusicTextRun author;
    std::string collaborators;
    std::string year;
    std::string privacy = "PUBLIC";
    std::string duration;
    std::string views;
    std::optional<std::int64_t> track_count;
    bool owned = false;
    std::vector<MusicThumbnail> thumbnails;
    std::vector<MusicItem> tracks;
    std::optional<MusicContinuation> continuation;
};

void to_json(json& data, const MusicPlaylistRequest& value);
void from_json(const json& data, MusicPlaylistPage& value);
void from_json(const json& data, MusicPlaylist& value);
