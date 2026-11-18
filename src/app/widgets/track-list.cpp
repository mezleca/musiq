#include "track-list.hpp"
#include "catalog-card.hpp"
#include "../../youtube/schema/common.hpp"
#include <arbor/input/router.hpp>
#include <arbor/surface.hpp>
#include <arbor/widgets/context-menu.hpp>

#include <algorithm>
#include <string>
#include <utility>

TrackList::TrackList(std::string id) : VirtualLayout(std::move(id), 64.0f) {
    set_type_name("TrackList");
}

MediaCardWidget&
TrackList::add_track(std::uint64_t key, const MusicItem& item, ThumbnailCache& thumbnails, bool loading, std::size_t number) {
    auto& card = add<MediaCardWidget>(item, thumbnails, loading, number);
    card.set_id(std::to_string(key));
    card.set_selected(!m_current.empty() && m_current == item.endpoint.video_id);
    m_rows.emplace(key, TrackRow{item.endpoint.video_id, &card, m_update});
    return card;
}

MediaCardWidget* TrackList::find_track(std::uint64_t key) const {
    const auto row = m_rows.find(key);
    if (row == m_rows.end()) return nullptr;
    row->second.last_update = m_update;
    return row->second.card;
}

static bool has_visible_menu(const ui::Node& node) {
    if (const auto* menu = dynamic_cast<const ui::ContextMenuWidget*>(&node); menu && menu->is_open()) {
        return true;
    }

    for (const auto& child : node.children()) {
        if (has_visible_menu(*child)) {
            return true;
        }
    }

    return false;
}

void TrackList::on_update(float) {
    ++m_update;

    const auto& router = surface().input_router();
    const auto* focus = router.focused_node();

    // the provider requests rows during draw, so retirement allows two missed updates
    for (auto row = m_rows.begin(); row != m_rows.end();) {
        // keep rows alive until focus, capture and menus have finished
        if (m_update - row->second.last_update > 2 && !row->second.card->contains(router.captured_node()) &&
            !row->second.card->contains(focus) && !row->second.card->subtree_input_state().active &&
            !has_visible_menu(*row->second.card)) {
            remove(*row->second.card);
            row = m_rows.erase(row);
        } else {
            ++row;
        }
    }
}

void TrackList::retain(const std::function<bool(std::uint64_t)>& keep) {
    std::erase_if(m_rows, [&](const auto& row) {
        if (keep(row.first)) return false;
        remove(*row.second.card);
        return true;
    });
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
