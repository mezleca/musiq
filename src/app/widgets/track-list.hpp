#pragma once

#include <arbor/layout/virtual-layout.hpp>
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
    mutable std::uint64_t last_update = 0;
};

class TrackList : public ui::VirtualLayout {
public:
    explicit TrackList(std::string id = "tracks");

    MediaCardWidget& add_track(std::uint64_t key, const MusicItem& item, ThumbnailCache& thumbnails, bool loading = false);
    MediaCardWidget* find_track(std::uint64_t key) const;

    void retain(const std::function<bool(std::uint64_t)>& keep);
    void clear_tracks();
    void set_current(std::string_view video_id);

private:
    void on_update(float dt) override;

    std::unordered_map<std::uint64_t, TrackRow> m_rows;
    std::string m_current;
    std::uint64_t m_update = 0;
};

/// updates visible lists under root
void update_track_lists(ui::Node& root, std::string_view video_id);
