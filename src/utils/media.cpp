#include "media.hpp"
#include "../youtube/schema/common.hpp"
#include "../http/http-client.hpp"
#include "log.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

using namespace utils::media;

bool utils::media::is_playlist_card(const MusicItem& item) {
    return item.endpoint.video_id.empty() && (is_album(item.endpoint) || item.endpoint.page_type == "MUSIC_PAGE_TYPE_PLAYLIST" ||
                                              item.endpoint.browse_id.starts_with("VL"));
}

std::string utils::media::subtitle_part(const MusicItem& item, int index) {
    std::string text;
    for (const auto& run : item.subtitle) {
        if (run.text == " • " || run.text == " · ") {
            if (index == 0) break;
            --index;
        } else if (index == 0) {
            text += run.text;
        }
    }

    return text;
}

// prefer linked creators, falling back to the artist segment when the response has no channel link
std::string utils::media::media_artist(const MusicItem& item) {
    std::string artist;
    for (const auto& run : item.subtitle) {
        if (!run.endpoint.browse_id.starts_with("UC")) continue;
        if (!artist.empty()) artist += ", ";
        artist += run.text;
    }

    if (!artist.empty()) return artist;

    auto first = subtitle_part(item);
    if (first == "Song" || first == "Video" || first == "Album" || first == "EP" || first == "Single" || first == "Playlist") {
        return subtitle_part(item, 1);
    }

    return first;
}

const MusicThumbnail* utils::media::select_thumbnail(std::span<const MusicThumbnail> thumbnails, float pixel_size) {
    const MusicThumbnail* selected = nullptr;
    int selected_extent = 0;

    for (const auto& candidate : thumbnails) {
        if (candidate.url.empty()) continue;

        const int extent = std::min(candidate.width, candidate.height);
        if (!selected) {
            selected = &candidate;
            selected_extent = extent;
            continue;
        }

        const bool closer =
            selected_extent < pixel_size ? extent > selected_extent : extent >= pixel_size && extent < selected_extent;
        if (closer) {
            selected = &candidate;
            selected_extent = extent;
        }
    }

    return selected;
}

std::string utils::media::fetch_thumbnail(const std::string& url) {
    HttpRequest request;
    request.url = url;
    request.timeout = std::chrono::seconds(10);
    request.connect_timeout = std::chrono::seconds(5);
    request.follow_redirects = true;

    auto response = http::request(request);
    if (!http::successful(response)) {
        LOG_ERROR(
            "[artwork] thumbnail download failed (status={}, transport={}): {}", response.status_code,
            static_cast<int>(response.error.code), url
        );
        return {};
    }

    return std::move(response.text);
}
