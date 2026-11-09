#pragma once

#include <span>
#include <string>

struct MusicItem;
struct MusicThumbnail;

namespace utils::media {
    bool is_playlist_card(const MusicItem& item);
    std::string subtitle_part(const MusicItem& item, int index = 0);
    std::string media_artist(const MusicItem& item);

    // choose the smallest variant that fills a square in physical pixels, or the largest available
    const MusicThumbnail* select_thumbnail(std::span<const MusicThumbnail> thumbnails, float pixel_size);
    std::string fetch_thumbnail(const std::string& url);
} // namespace utils::media
