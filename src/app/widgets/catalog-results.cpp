#include "catalog-results.hpp"
#include "text.hpp"
#include "catalog-card.hpp"
#include "track-list.hpp"
#include "../../youtube/catalog-browser.hpp"
#include "../../utils/media.hpp"

#include <arbor/widgets/context-menu.hpp>

#include <algorithm>
#include <ranges>
#include <utility>

using namespace ui;
using namespace utils::media;

class CatalogTracksWidget final : public TrackList {
public:
    CatalogTracksWidget(
        std::shared_ptr<const MusicPage> page, std::size_t shelf, std::size_t first, std::size_t count,
        ThumbnailCache& thumbnails, CatalogActions actions
    )
        : TrackList("tracks"), m_actions(std::move(actions)) {
        set_size({grow(), fit()});
        set_spacing(8.0f);

        // keep the page alive while cards and their callbacks reference its items
        set_items(count, [this, page, shelf, first, &thumbnails](std::size_t index) -> Node& {
            if (auto* card = find_track(index)) {
                return *card;
            }

            const auto* item = &page->shelves[shelf].items[first + index];
            auto& card = add_track(index, *item, thumbnails);
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

            return card;
        });
    }

private:
    CatalogActions m_actions;
};

class PlaylistGridWidget final : public Container {
public:
    PlaylistGridWidget(
        std::shared_ptr<const MusicPage> page, std::size_t shelf, std::size_t first, std::size_t count,
        ThumbnailCache& thumbnails, std::function<void(MusicItem)> play
    )
        : Container("playlists", StackDirection::Horizontal) {
        set_size({grow(), fit()});
        set_spacing(16.0f);
        set_wrap(StackWrap::Wrap);

        for (std::size_t index = first; index < first + count; ++index) {
            const auto* item = &page->shelves[shelf].items[index];
            auto& card = add<PlaylistCardWidget>(*item, thumbnails);
            // keep the source alive with the callback so its item pointer remains valid
            card.on_click([page, item, play] { play(*item); });
        }
    }
};

CatalogResultsWidget::CatalogResultsWidget(std::string id, std::string title, ThumbnailCache& thumbnails)
    : Container(std::move(id), StackDirection::Vertical), m_thumbnails(thumbnails) {
    set_type_name("CatalogResultsWidget");
    set_size({grow(), grow()});
    set_spacing(16.0f);

    auto& heading = add<MusiqText>(std::move(title), FontWeight::Bold, 22.0f);
    heading.set_size({grow(), fit()});
    heading.set_overflow(TextOverflow::Ellipsis);
    m_status = &add<MusiqText>("");

    m_rows = &add<Container>("results", StackDirection::Vertical);
    m_rows->set_size({grow(), grow()});
    m_rows->set_spacing(20.0f);
    m_rows->set_scrollable(true);
}

void CatalogResultsWidget::set_actions(CatalogActions actions) {
    m_actions = std::move(actions);
}

bool CatalogResultsWidget::near_bottom() const {
    return m_rows->layout().size().y > 0.0f && m_rows->scroll().max().y - m_rows->scroll().position().y < 400.0f;
}

void CatalogResultsWidget::show(const CatalogPageState& state) {
    if (m_page != state.page) {
        m_rows->clear();
        m_page = state.page;
        m_shelf_count = 0;
        m_rows->scroll().seek_to({0.0f, 0.0f}, ScrollBehavior::Instant);
    }

    rebuild(m_shelf_count);
    m_shelf_count = m_page ? m_page->shelves.size() : 0;

    if (state.loading() && !m_page) {
        if (m_rows->find("loading") == nullptr) {
            auto& tracks = m_rows->add<TrackList>("loading");
            tracks.set_size({grow(), fit()});
            tracks.set_spacing(8.0f);
            tracks.set_items(12, [&tracks, this](std::size_t index) -> Node& {
                if (auto* card = tracks.find_track(index)) return *card;
                return tracks.add_track(index, MusicItem{}, m_thumbnails, true);
            });
        }
    } else if (auto* loading = m_rows->find("loading")) {
        m_rows->remove(*loading);
    }

    std::string status;

    if (!state.error.empty()) {
        status = state.error;
    } else if (state.loading()) {
        if (state.page) status = "Loading...";
    } else if (!state.page && state.query.empty()) {
        status = "Type a query and press Enter.";
    } else if (!m_page || std::ranges::none_of(m_page->shelves, [](const MusicShelf& shelf) { return !shelf.items.empty(); })) {
        status = "No results found.";
    }

    m_status->set_text(std::move(status));
    m_status->set_visible(!m_status->empty());
}

void CatalogResultsWidget::rebuild(std::size_t first_shelf) {
    if (!m_page) {
        return;
    }

    const auto same_card_type = [](const MusicItem& left, const MusicItem& right) {
        return is_playlist_card(left) == is_playlist_card(right);
    };

    for (std::size_t shelf_index = first_shelf; shelf_index < m_page->shelves.size(); ++shelf_index) {
        const auto& shelf = m_page->shelves[shelf_index];
        if (shelf.items.empty()) {
            continue;
        }

        auto& section = m_rows->add<Container>("", StackDirection::Vertical);
        section.set_size({grow(), fit()});
        section.set_spacing(8.0f);

        if (!shelf.title.empty()) {
            section.add<MusiqText>(shelf.title, FontWeight::Bold, 22.0f);
        }

        for (const auto group : std::views::chunk_by(shelf.items, same_card_type)) {
            const auto first = static_cast<std::size_t>(group.begin() - shelf.items.begin());
            if (is_playlist_card(group.front())) {
                section.add<PlaylistGridWidget>(m_page, shelf_index, first, group.size(), m_thumbnails, m_actions.play);
            } else {
                section.add<CatalogTracksWidget>(m_page, shelf_index, first, group.size(), m_thumbnails, m_actions);
            }
        }
    }
}

void CatalogResultsWidget::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);

    configure_all_styles([](Style& style) { style.padding({16.0f, 0.0f, 16.0f, 0.0f}); });
    m_rows->configure_all_styles([](Style& style) { style.padding({12.0f, 0.0f, 12.0f, 0.0f}); });
}
