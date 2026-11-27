#include "catalog-results.hpp"
#include "text.hpp"
#include "catalog-card.hpp"
#include "track-list.hpp"
#include "../../youtube/catalog-browser.hpp"
#include "../../utils/media.hpp"

#include <utility>

using namespace ui;
using namespace utils::media;

static constexpr float HomeTrackWidth = 260.0f;

static void bind_track(MediaCardWidget& card, const MusicItem& item, const CatalogActions& actions) {
    card.set_enabled(is_playable(item));
    card.on_click([&item, &actions] { actions.play(item); });
    if (actions.menu) {
        attach_context_menu(card, [&item, &actions](ImVec2 position) { return actions.menu(item, {}, position); });
    }
}

class CatalogTracksWidget final : public TrackList {
public:
    CatalogTracksWidget(
        std::shared_ptr<const MusicPage> page, std::size_t shelf, std::size_t first, std::size_t count,
        ThumbnailCache& thumbnails, const CatalogActions& actions
    )
        : TrackList("tracks") {
        set_size({grow(), fit()});
        set_spacing(2.0f);

        set_items(count, [this, page, shelf, first, &thumbnails, &actions](std::size_t index) -> Node& {
            if (auto* existing = find_track(index)) {
                return *existing;
            }

            const auto& item = page->shelves[shelf].items[first + index];
            auto& card = add_track(index, item, thumbnails);
            bind_track(card, item, actions);
            return card;
        });
    }
};

class CatalogTilesWidget final : public Container {
public:
    CatalogTilesWidget(
        std::shared_ptr<const MusicPage> page, std::size_t shelf, std::size_t first, std::size_t count,
        ThumbnailCache& thumbnails, const CatalogActions& actions
    )
        : Container("tiles", StackDirection::Horizontal) {
        set_size({grow(), fit()});
        set_spacing(24.0f);
        set_wrap(StackWrap::Wrap);

        for (std::size_t index = first; index < first + count; ++index) {
            append(page, page->shelves[shelf].items[index], thumbnails, actions);
        }
    }

    void append(
        std::shared_ptr<const MusicPage> page, const MusicItem& item, ThumbnailCache& thumbnails, const CatalogActions& actions
    ) {
        OpenMenu open;
        if (actions.menu) {
            open = [page, item = &item, menu = actions.menu](ImVec2 position) { return menu(*item, {}, position); };
        }

        const bool playlist = is_playlist_card(item);
        std::function<void()> play_playlist;
        if (playlist) play_playlist = [item = &item, play = actions.play_playlist] { play(*item); };
        auto& card = add<CatalogTileWidget>(item, thumbnails, std::move(open), std::move(play_playlist));
        card.set_enabled(playlist || is_playable(item));
        card.on_click([item = &item, play = actions.play] { play(*item); });
    }
};

CatalogResultsWidget::CatalogResultsWidget(
    std::string id, std::string title, ThumbnailCache& thumbnails, CatalogActions actions, bool home
)
    : Container(std::move(id), StackDirection::Vertical), m_thumbnails(thumbnails), m_actions(std::move(actions)), m_home(home) {
    set_type_name("CatalogResultsWidget");
    set_size({grow(), grow()});
    set_spacing(16.0f);

    if (!title.empty()) {
        auto& heading = add<MusiqText>(std::move(title), FontWeight::Bold, 22.0f);
        heading.set_size({grow(), fit()});
        heading.set_overflow(TextOverflow::Ellipsis);
    }

    m_status = &add<MusiqText>("");

    m_rows = &add<Container>("results", StackDirection::Vertical);
    m_rows->set_size({grow(), grow()});
    m_rows->set_spacing(24.0f);
    m_rows->set_scrollable(true);
}

bool CatalogResultsWidget::near_bottom() const {
    return m_rows->layout().size().y > 0.0f && m_rows->scroll().max().y - m_rows->scroll().position().y < 400.0f;
}

void CatalogResultsWidget::show(const CatalogPageState& state) {
    if (m_page != state.page) {
        m_rows->clear();
        m_singleton_playlists = nullptr;
        m_page = state.page;
        m_shelf_count = 0;
        m_rows->scroll().seek_to({0.0f, 0.0f}, ScrollBehavior::Instant);
    }

    rebuild();

    if (state.loading && !m_page) {
        if (m_rows->find("loading") == nullptr) {
            auto& tracks = m_rows->add<TrackList>("loading");
            tracks.set_size({grow(), fit()});
            tracks.set_spacing(2.0f);
            tracks.set_items(12, [&tracks, this](std::size_t index) -> Node& {
                if (auto* card = tracks.find_track(index)) {
                    return *card;
                }
                return tracks.add_track(index, MusicItem{}, m_thumbnails, true);
            });
        }
    } else if (auto* loading = m_rows->find("loading")) {
        m_rows->remove(*loading);
    }

    std::string status;

    if (!state.error.empty()) {
        status = state.error;
    } else if (state.loading) {
        if (state.page) status = "Loading...";
    } else if (!state.page) {
        status = "Type a query and press Enter.";
    } else if (!m_page || std::ranges::none_of(m_page->shelves, [](const MusicShelf& shelf) { return !shelf.items.empty(); })) {
        status = "No results found.";
    }

    m_status->set_text(std::move(status));
    m_status->set_visible(!m_status->empty());
}

void CatalogResultsWidget::rebuild() {
    if (m_page == nullptr) {
        return;
    }

    const auto add_track = [this](Container& parent, const MusicItem& item) -> MediaCardWidget& {
        auto& card = parent.add<MediaCardWidget>(item, m_thumbnails);
        card.set_id(item.endpoint.video_id);
        bind_track(card, item, m_actions);
        return card;
    };

    for (; m_shelf_count < m_page->shelves.size(); ++m_shelf_count) {
        const auto shelf_index = m_shelf_count;
        const auto& shelf = m_page->shelves[shelf_index];
        if (shelf.items.empty()) {
            m_singleton_playlists = nullptr;
            continue;
        }

        if (!shelf.carousel && shelf.title.empty() && shelf.items.size() == 1 && !shelf.continuation) {
            const auto* item = &shelf.items.front();
            if (is_playlist_card(*item)) {
                if (m_singleton_playlists) {
                    m_singleton_playlists->append(m_page, *item, m_thumbnails, m_actions);
                } else {
                    m_singleton_playlists = &m_rows->add<CatalogTilesWidget>(m_page, shelf_index, 0, 1, m_thumbnails, m_actions);
                }
            } else {
                m_singleton_playlists = nullptr;
                add_track(*m_rows, *item);
            }

            continue;
        }

        m_singleton_playlists = nullptr;
        auto& section = m_rows->add<Container>("", StackDirection::Vertical);
        section.set_size({grow(), fit()});
        section.set_spacing(10.0f);

        if (!shelf.title.empty()) {
            section.configure_all_styles([](Style& style) { style.padding_bottom(8.0f); });
            section.add<MusiqText>(shelf.title, FontWeight::Bold, 22.0f);
        }

        if (shelf.carousel && !m_home) {
            section.add<CatalogTilesWidget>(m_page, shelf_index, 0, shelf.items.size(), m_thumbnails, m_actions);
            continue;
        }

        const auto square_tile = [&shelf](const MusicItem& item) {
            return is_playlist_card(item) || (shelf.carousel && item.endpoint.video_id.empty());
        };

        for (std::size_t first = 0; first < shelf.items.size();) {
            const bool square = square_tile(shelf.items[first]);

            std::size_t end = first + 1;
            while (end < shelf.items.size() && square_tile(shelf.items[end]) == square) {
                ++end;
            }

            if (square) {
                section.configure_all_styles([](Style& style) { style.padding_bottom(8.0f); });
                section.add<CatalogTilesWidget>(m_page, shelf_index, first, end - first, m_thumbnails, m_actions);
            } else if (m_home && end - first > 1) {
                for (auto index = first; index < end; index += 2) {
                    auto& row = section.add<Container>("tracks", StackDirection::Horizontal);
                    row.set_size({grow(), fit()});
                    row.set_spacing(10.0f);
                    row.set_wrap(StackWrap::Wrap);

                    for (auto track = index; track < std::min(index + 2, end); ++track) {
                        add_track(row, shelf.items[track]).set_size({{grow(), px(HomeTrackWidth), percent(100.0f)}, fit()});
                    }
                }
            } else if (end - first == 1) {
                add_track(section, shelf.items[first]);
            } else {
                section.add<CatalogTracksWidget>(m_page, shelf_index, first, end - first, m_thumbnails, m_actions);
            }

            first = end;
        }
    }
}

void CatalogResultsWidget::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);

    configure_all_styles([](Style& style) { style.padding({12.0f, 0.0f}); });
    m_rows->configure_all_styles([](Style& style) { style.padding({0.0f, 12.0f}); });
}
