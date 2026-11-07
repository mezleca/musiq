#pragma once

#include "common.hpp"

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/browsing.html#ytmusicapi.YTMusic.get_song
struct MusicPlayerRequest {
    std::string video_id;
    std::string playlist_id;
    std::optional<int> signature_timestamp;
    std::string po_token;
};

struct MusicAudioTrack {
    bool is_auto_dubbed = false;
    bool is_default = false;
};

// ref: https://ytjs.dev/api/youtubei.js/namespaces/Misc/classes/Format#properties
struct MusicFormat {
    int itag = 0;
    std::string mime_type;
    std::int64_t bitrate = 0;
    std::string url;
    std::string signature_cipher;
    std::optional<std::int64_t> content_length;
    std::optional<MusicAudioTrack> audio_track;
    std::vector<std::string> drm_families;
    std::string drm_track_type;
};

struct MusicStreamingData {
    std::optional<std::int64_t> expires_in_seconds;
    std::vector<MusicFormat> formats;
    std::vector<MusicFormat> adaptive_formats;
};

struct MusicVideoDetails {
    std::string video_id;
    std::string title;
    std::string author;
    std::string channel_id;
    std::optional<std::int64_t> length_seconds;
    bool is_live = false;
    std::vector<MusicThumbnail> thumbnails;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/browsing.html#ytmusicapi.YTMusic.get_song
struct MusicPlayerData {
    std::string status;
    std::string reason;
    std::optional<MusicStreamingData> streaming_data;
    std::optional<MusicVideoDetails> video_details;
};

struct MusicVisitorData {
    std::string visitor_data;
};

void to_json(json& data, const MusicPlayerRequest& value);
void from_json(const json& data, MusicAudioTrack& value);
void to_json(json& data, const MusicAudioTrack& value);
void from_json(const json& data, MusicFormat& value);
void to_json(json& data, const MusicFormat& value);
void from_json(const json& data, MusicStreamingData& value);
void to_json(json& data, const MusicStreamingData& value);
void from_json(const json& data, MusicVideoDetails& value);
void to_json(json& data, const MusicVideoDetails& value);
void from_json(const json& data, MusicPlayerData& value);
void to_json(json& data, const MusicPlayerData& value);
void from_json(const json& data, MusicVisitorData& value);
void to_json(json& data, const MusicVisitorData& value);
