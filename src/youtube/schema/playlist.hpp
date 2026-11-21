#pragma once

#include "common.hpp"
#include "continuation.hpp"

#include <filesystem>
#include <string>
#include <string_view>

std::string playlist_browse_id(std::string_view playlist_id);
// the returned view borrows the input storage
std::string_view playlist_write_id(std::string_view playlist_id);

enum class MusicPlaylistPrivacy : uint8_t {
    Private,
    Unlisted,
    Public,
};

std::string_view playlist_privacy_value(MusicPlaylistPrivacy privacy);
std::optional<MusicPlaylistPrivacy> playlist_privacy_from(std::string_view value);

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/playlists.html#ytmusicapi.YTMusic.get_playlist
struct MusicPlaylistRequest {
    std::string playlist_id;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/playlists.html#ytmusicapi.YTMusic.create_playlist
struct MusicPlaylistCreateRequest {
    std::string title;
    std::string description;
    MusicPlaylistPrivacy privacy = MusicPlaylistPrivacy::Private;
    std::vector<std::string> video_ids;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/playlists.html#ytmusicapi.YTMusic.add_playlist_items
struct MusicPlaylistAddRequest {
    std::string playlist_id;
    std::vector<std::string> video_ids;

    bool allow_duplicates = false;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/playlists.html#ytmusicapi.YTMusic.remove_playlist_items
struct MusicPlaylistRemoveRequest {
    struct Entry {
        std::string video_id;
        std::string set_video_id;
    };

    std::string playlist_id;
    std::vector<Entry> entries;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/playlists.html#ytmusicapi.YTMusic.edit_playlist
struct MusicPlaylistUpdateRequest {
    std::string playlist_id;
    std::optional<std::string> title;
    std::optional<std::string> description;
    std::optional<MusicPlaylistPrivacy> privacy;
    std::optional<std::filesystem::path> thumbnail_path;
    std::optional<std::string> thumbnail_blob_id;
    bool remove_thumbnail = false;
};

struct MusicPlaylistCreated {
    std::string id;
    std::string status;
};

struct MusicPlaylistAddedEntry {
    std::string video_id;
    std::string set_video_id;
};

struct MusicPlaylistEditResult {
    std::string status;
    std::vector<MusicPlaylistAddedEntry> added;
};

struct MusicPlaylistPage {
    std::vector<MusicItem> tracks;
    std::optional<MusicContinuation> continuation;
};

struct MusicPlaylistRecommendations {
    std::vector<MusicItem> tracks;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/playlists.html#ytmusicapi.YTMusic.get_playlist
struct MusicPlaylist {
    std::string id;
    std::string title;
    std::string description;
    MusicTextRun author;
    std::string collaborators;
    std::string year;
    MusicPlaylistPrivacy privacy = MusicPlaylistPrivacy::Public;
    std::string duration;
    std::string views;
    std::optional<std::int64_t> track_count;
    bool owned = false;
    std::vector<MusicThumbnail> thumbnails;
    std::vector<MusicItem> tracks;
    std::optional<MusicContinuation> continuation;
    std::optional<MusicContinuation> recommendations_continuation;
};

void to_json(json& data, const MusicPlaylistRequest& value);
void to_json(json& data, const MusicPlaylistCreateRequest& value);
void to_json(json& data, const MusicPlaylistAddRequest& value);
void to_json(json& data, const MusicPlaylistRemoveRequest& value);
void to_json(json& data, const MusicPlaylistUpdateRequest& value);
void from_json(const json& data, MusicPlaylistPage& value);
void from_json(const json& data, MusicPlaylistRecommendations& value);
void from_json(const json& data, MusicPlaylist& value);
void from_json(const json& data, MusicPlaylistCreated& value);
void from_json(const json& data, MusicPlaylistEditResult& value);
