#pragma once

#include "../../utils/json.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct MusicThumbnail {
    std::string url;
    int width = 0;
    int height = 0;
};

// ref: https://github.com/LuanRT/YouTube.js/blob/main/src/parser/classes/MusicTwoRowItem.ts#L50
struct MusicEndpoint {
    std::string video_id;
    std::string playlist_id;
    std::string browse_id;
    std::string params;
    std::string page_type;
};

struct MusicTextRun {
    std::string text;
    MusicEndpoint endpoint;
};

// ref: https://ytjs.dev/api/youtubei.js/namespaces/YTNodes/classes/MusicResponsiveListItem#properties
struct MusicItem {
    std::string title;
    std::vector<MusicTextRun> subtitle;
    MusicEndpoint endpoint;
    std::vector<MusicThumbnail> thumbnails;
    std::string duration;
    std::string playlist_set_video_id;
    MusicTextRun album;
    bool available = true;
};

inline bool is_playable(const MusicItem& item) {
    return item.available && !item.endpoint.video_id.empty();
}

inline bool is_album(const MusicEndpoint& endpoint) {
    return endpoint.page_type == "MUSIC_PAGE_TYPE_ALBUM" || endpoint.browse_id.starts_with("MPRE");
}

std::string music_text(const json& data);
std::vector<MusicTextRun> music_text_runs(const json& data);
std::int64_t music_number(const json& data);
std::optional<std::int64_t> music_optional_number(const json& data, std::string_view key);

template <typename T>
std::optional<T> music_optional(const json& data, std::string_view key) {
    const auto field = data.find(key);
    if (field == data.end() || field->is_null()) return std::nullopt;
    return field->get<T>();
}

void from_json(const json& data, MusicThumbnail& value);
void to_json(json& data, const MusicThumbnail& value);
void from_json(const json& data, MusicEndpoint& value);
void to_json(json& data, const MusicEndpoint& value);
void from_json(const json& data, MusicTextRun& value);
void to_json(json& data, const MusicTextRun& value);
void from_json(const json& data, MusicItem& value);
void to_json(json& data, const MusicItem& value);
