#include "catalog-results.hpp"
#include "text.hpp"
#include "catalog-card.hpp"
#include "../catalog-browser.hpp"

#include <utility>

using namespace ui;

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

void CatalogResultsWidget::show(const CatalogPageState& state) {
    m_rows->clear();
    std::size_t count = 0;

    if (state.page) {
        for (const auto& shelf : state.page->shelves) {
            if (shelf.items.empty()) continue;

            auto& section = m_rows->add<Container>("", StackDirection::Vertical);
            section.set_size({grow(), fit()});
            section.set_spacing(8.0f);
            if (!shelf.title.empty()) section.add<MusiqText>(shelf.title, true);

            Container* playlists = nullptr;
            for (const auto& item : shelf.items) {
                if (is_playlist_card(item)) {
                    if (!playlists) {
                        playlists = &section.add<Container>("", StackDirection::Horizontal);
                        playlists->set_size({grow(), fit()});
                        playlists->set_wrap(StackWrap::Wrap);
                        playlists->set_spacing(16.0f);
                    }
                    playlists->add<PlaylistCardWidget>(item, m_thumbnails);
                } else {
                    playlists = nullptr;
                    section.add<MediaCardWidget>(item, m_thumbnails);
                }
            }
            count += shelf.items.size();
        }
    }

    std::string status;
    if (!state.error.empty()) {
        status = state.error;
    } else if (state.loading) {
        status = "Loading...";
    } else if (!state.page && state.query.empty()) {
        status = "Type a search and press Enter.";
    } else if (count == 0) {
        status = "No results found.";
    }

    m_status->set_text(std::move(status));
    m_status->set_visible(!m_status->empty());
    m_rows->scroll().seek_to({0.0f, 0.0f}, ScrollBehavior::Instant);
}

void CatalogResultsWidget::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    configure_all_styles([](Style& style) { style.padding({16.0f, 0.0f, 16.0f, 16.0f}); });
    m_rows->configure_all_styles([](Style& style) { style.padding({12.0f, 0.0f, 12.0f, 0.0f}); });
}
