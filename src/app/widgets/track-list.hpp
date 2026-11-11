#pragma once

#include <imgui-ui/layout/virtual-layout.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

class MediaCardWidget;
class ThumbnailCache;
struct MusicItem;

struct TrackRow {
    std::string video_id;
    MediaCardWidget* card;
};

class TrackList : public ui::VirtualLayout {
public:
    explicit TrackList(std::string id = "tracks");

    MediaCardWidget& add_track(std::uint64_t key, const MusicItem& item, ThumbnailCache& thumbnails);
    MediaCardWidget* find_track(std::uint64_t key) const;

    void retain(const std::function<bool(std::uint64_t)>& keep);
    void clear_tracks();
    void set_current(std::string_view video_id);

private:
    std::unordered_map<std::uint64_t, TrackRow> m_rows;
    std::string m_current;
};

/// updates visible lists under root
void update_track_lists(ui::Node& root, std::string_view video_id);
