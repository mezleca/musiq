#include "catalog-results.hpp"
#include "text.hpp"
#include "catalog-card.hpp"
#include "../catalog-browser.hpp"

#include <imgui-ui/widgets/context-menu.hpp>
#include <imgui-ui/layout/virtual-layout.hpp>

#include <algorithm>
#include <utility>
#include <unordered_map>

using namespace ui;

class CatalogTracksWidget final : public VirtualLayout {
public:
    CatalogTracksWidget(
        std::shared_ptr<const MusicPage> page, std::size_t shelf, std::size_t first, std::size_t count,
        ThumbnailCache& thumbnails, CatalogActions actions
    )
        : VirtualLayout("tracks", 64.0f), m_actions(std::move(actions)) {
        set_size({grow(), fit()});
        set_spacing(8.0f);

        // keep the page alive while cards and their callbacks reference its items
        set_items(count, [this, page, shelf, first, &thumbnails](std::size_t index) -> Node& {
            auto [entry, created] = m_rows.try_emplace(index, nullptr);
            if (!created) return *entry->second;

            const auto* item = &page->shelves[shelf].items[first + index];
            auto& card = add<MediaCardWidget>(*item, thumbnails);
            card.set_enabled(is_playable(*item));
            card.on_click([this, item] { m_actions.play(*item); });

            auto& menu = card.add<ContextMenuWidget>(ContextMenuItems{
                ContextMenuItem::action("Play next", [this, item](ContextMenuWidget&) { m_actions.play_next(*item); }),
                ContextMenuItem::action("Add to queue", [this, item](ContextMenuWidget&) { m_actions.append(*item); })
            });
            card.on_event(EventType::ContextClick, [&menu](UiEvent& event) {
                menu.open_at(event.position);
                event.stop_propagation();
            });
            entry->second = &card;
            return card;
        });
    }

private:
    CatalogActions m_actions;
    std::unordered_map<std::size_t, MediaCardWidget*> m_rows;
};

class PlaylistGridWidget final : public VirtualLayout {
public:
    PlaylistGridWidget(
        std::shared_ptr<const MusicPage> page, std::size_t shelf, std::size_t first, std::size_t count,
        ThumbnailCache& thumbnails, std::function<void(MusicItem)> play
    )
        : VirtualLayout("playlists", 236.0f), m_play(std::move(play)), m_count(count) {
        set_size({grow(), fit()});
        set_spacing(16.0f);

        set_items(0, [this, page, shelf, first, &thumbnails](std::size_t index) -> Node& {
            auto [entry, created] = m_rows.try_emplace(index, nullptr);
            if (!created) return *entry->second;

            auto& row = add<Container>("", StackDirection::Horizontal);
            row.set_size({grow(), fit()});
            row.set_spacing(16.0f);

            const auto end = std::min(m_count, (index + 1) * m_columns);
            for (auto item_index = index * m_columns; item_index < end; ++item_index) {
                const auto* item = &page->shelves[shelf].items[first + item_index];
                auto& card = row.add<PlaylistCardWidget>(*item, thumbnails);
                card.on_click([this, item] { m_play(*item); });
            }
            entry->second = &row;
            return row;
        });
    }

protected:
    void on_update(float) override {
        // layout width is still zero on the first frame, wait for the measured size
        const float width = layout().size().x;
        if (width <= 0.0f) return;

        const auto columns = std::max(std::size_t{1}, static_cast<std::size_t>(width + 16.0f) / 192);
        if (columns == m_columns) return;

        m_columns = columns;
        m_rows.clear();
        clear();
        set_items((m_count + columns - 1) / columns);
    }

private:
    std::function<void(MusicItem)> m_play;
    std::unordered_map<std::size_t, Container*> m_rows;
    std::size_t m_count;
    std::size_t m_columns = 0;
};

CatalogResultsWidget::CatalogResultsWidget(std::string title, ThumbnailCache& thumbnails)
    : Container("catalog-results", StackDirection::Vertical), m_thumbnails(thumbnails) {
    set_type_name("CatalogResultsWidget");
    set_size({grow(), grow()});
    set_spacing(16.0f);

    m_title = &add<MusiqText>(std::move(title), true);
    m_title->set_size({grow(), fit()});
    m_title->set_overflow(TextOverflow::Ellipsis);
    m_status = &add<MusiqText>("");

    m_rows = &add<Container>("results", StackDirection::Vertical);
    m_rows->set_size({grow(), grow()});
    m_rows->set_spacing(20.0f);
    m_rows->set_scrollable(true);
}

void CatalogResultsWidget::set_title(std::string title) {
    m_title->set_text(std::move(title));
}

void CatalogResultsWidget::set_actions(CatalogActions actions) {
    m_actions = std::move(actions);
}

void CatalogResultsWidget::show(const CatalogPageState& state) {
    if (m_page != state.page) {
        m_rows->clear();
        m_page = state.page;
        rebuild();
        m_rows->scroll().seek_to({0.0f, 0.0f}, ScrollBehavior::Instant);
    }

    std::string status;
    if (!state.error.empty()) {
        status = state.error;
    } else if (state.loading) {
        status = "Loading...";
    } else if (!state.page && state.query.empty()) {
        status = "Type a search and press Enter.";
    } else if (!m_page || std::ranges::none_of(m_page->shelves, [](const MusicShelf& shelf) { return !shelf.items.empty(); })) {
        status = "No results found.";
    }

    m_status->set_text(std::move(status));
    m_status->set_visible(!m_status->empty());
}

void CatalogResultsWidget::rebuild() {
    if (!m_page) return;

    for (std::size_t shelf_index = 0; shelf_index < m_page->shelves.size(); ++shelf_index) {
        const auto& shelf = m_page->shelves[shelf_index];
        if (shelf.items.empty()) continue;

        auto& section = m_rows->add<Container>("", StackDirection::Vertical);
        section.set_size({grow(), fit()});
        section.set_spacing(8.0f);
        if (!shelf.title.empty()) section.add<MusiqText>(shelf.title, true);

        for (std::size_t first = 0; first < shelf.items.size();) {
            const bool playlists = is_playlist_card(shelf.items[first]);
            auto end = first + 1;
            while (end < shelf.items.size() && is_playlist_card(shelf.items[end]) == playlists) {
                ++end;
            }

            if (playlists) {
                section.add<PlaylistGridWidget>(m_page, shelf_index, first, end - first, m_thumbnails, m_actions.play);
            } else {
                section.add<CatalogTracksWidget>(m_page, shelf_index, first, end - first, m_thumbnails, m_actions);
            }
            first = end;
        }
    }
}

void CatalogResultsWidget::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    configure_all_styles([](Style& style) { style.padding({16.0f, 0.0f, 16.0f, 16.0f}); });
    m_rows->configure_all_styles([](Style& style) { style.padding({12.0f, 0.0f, 12.0f, 0.0f}); });
}
