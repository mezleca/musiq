#include "track-list.hpp"
#include "catalog-card.hpp"
#include "../../youtube/schema/common.hpp"

#include <string>
#include <utility>

TrackList::TrackList(std::string id) : VirtualLayout(std::move(id), 64.0f) {
    set_type_name("TrackList");
}

MediaCardWidget& TrackList::add_track(std::uint64_t key, const MusicItem& item, ThumbnailCache& thumbnails) {
    auto& card = add<MediaCardWidget>(item, thumbnails);
    card.set_id(std::to_string(key));
    card.set_selected(!m_current.empty() && m_current == item.endpoint.video_id);
    m_rows.emplace(key, TrackRow{item.endpoint.video_id, &card});
    return card;
}

MediaCardWidget* TrackList::find_track(std::uint64_t key) const {
    const auto row = m_rows.find(key);
    return row == m_rows.end() ? nullptr : row->second.card;
}

void TrackList::retain(const std::function<bool(std::uint64_t)>& keep) {
    for (auto row = m_rows.begin(); row != m_rows.end();) {
        const auto& [key, track] = *row;
        if (!keep(key)) {
            remove(*track.card);
            row = m_rows.erase(row);
            continue;
        }
        ++row;
    }
}

void TrackList::clear_tracks() {
    m_rows.clear();
    clear();
}

void TrackList::set_current(std::string_view video_id) {
    if (m_current == video_id) {
        return;
    }

    m_current = video_id;
    for (const auto& [key, row] : m_rows) {
        row.card->set_selected(!m_current.empty() && row.video_id == m_current);
    }
}

void update_track_lists(ui::Node& root, std::string_view video_id) {
    if (!root.visible() || root.removal_pending()) return;

    if (auto* tracks = dynamic_cast<TrackList*>(&root)) {
        tracks->set_current(video_id);
        return;
    }

    for (const auto& child : root.children()) {
        update_track_lists(*child, video_id);
    }
}
