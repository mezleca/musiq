#include "track-list.hpp"
#include "catalog-card.hpp"

#include <imgui-ui/layout/virtual-layout.hpp>
#include <string>

TrackList::TrackList(ui::VirtualLayout& layout) : m_layout(layout) {}

MediaCardWidget& TrackList::add(std::uint64_t key, const MusicItem& item, ThumbnailCache& thumbnails) {
    if (auto* card = find(key)) return *card;

    auto& card = m_layout.add<MediaCardWidget>(item, thumbnails);
    card.set_id(std::to_string(key));
    card.set_selected(m_current == key);
    m_rows.emplace(key, &card);
    return card;
}

MediaCardWidget* TrackList::find(std::uint64_t key) const {
    const auto row = m_rows.find(key);
    return row == m_rows.end() ? nullptr : row->second;
}

void TrackList::refresh(const std::function<bool(std::uint64_t)>& keep, std::optional<std::uint64_t> current) {
    for (auto row = m_rows.begin(); row != m_rows.end();) {
        const auto& [key, card] = *row;
        if (!keep(key)) {
            m_layout.remove(*card);
            row = m_rows.erase(row);
            continue;
        }
        card->set_selected(current == key);
        ++row;
    }

    m_current = current;
}

void TrackList::clear() {
    m_rows.clear();
    m_layout.clear();
    m_current.reset();
}

void TrackList::set_current(std::optional<std::uint64_t> key) {
    if (m_current == key) {
        return;
    }

    if (m_current) {
        if (auto* card = find(*m_current)) {
            card->set_selected(false);
        }
    }

    m_current = key;
    if (m_current) {
        if (auto* card = find(*m_current)) {
            card->set_selected(true);
        }
    }
}
