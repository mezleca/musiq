#include "sidebar.hpp"
#include "../widgets/options.hpp"
#include "../../youtube/schema/common.hpp"

#include <utility>

using namespace ui;

SideBarOverlay::SideBarOverlay(
    ThumbnailCache& thumbnails, std::function<void(std::string_view)> navigate, std::function<void(MusicItem)> open_playlist
)
    : ResizableContainer("sidebar"), m_thumbnails(thumbnails), m_navigate(std::move(navigate)),
      m_open_playlist(std::move(open_playlist)) {
    set_type_name("SideBarOverlay");
    set_resize(ResizeAxes::X);
    set_size({px(280), grow()});
    set_spacing(16.0f);

    m_options = &add<Container>("musiq-options", StackDirection::Vertical);
    m_options->set_size({grow(), fit()});
    m_options->set_spacing(4.0f);

    add_option("home", "Home", "icons/home.svg", true);
    add_option("search", "Search", "icons/search.svg");
    add_option("settings", "Settings", "icons/settings.svg");

    m_playlists = &add<Container>("user-playlists", StackDirection::Vertical);
    m_playlists->set_size({grow(), grow()});
    m_playlists->set_scrollable(true);
    m_playlists->set_spacing(4.0f);
}

void SideBarOverlay::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);

    configure_all_styles([&theme](Style& style) {
        style.padding({8.0f, 12.0f, 8.0f, 12.0f});
        style.border_radius(0.0f);
        style.border_color(theme.border_color);
        style.background_color(theme.background_secondary_color);
    });

    m_options->configure_all_styles([&theme](Style& style) {
        style.padding_bottom(16.0f).border_bottom(1.0f).border_radius(0.0f).border_color(theme.border_color);
    });
}

void SideBarOverlay::add_option(std::string id, std::string text, std::string icon_path, bool selected) {
    auto& option = m_options->add<OptionsWidget>(id, std::move(text), std::move(icon_path), selected);
    if (selected) {
        m_selected_option = &option;
    }

    option.on_event(EventType::Click, [this, id = std::move(id)](UiEvent&) {
        select(id);
        if (m_navigate) {
            m_navigate(id);
        }
    });
}

void SideBarOverlay::select(std::string_view id) {
    m_selected_id = id;

    auto* option = static_cast<OptionsWidget*>(m_options->find(id));
    if (!option) {
        option = static_cast<OptionsWidget*>(m_playlists->find(id));
    }

    if (option == m_selected_option) {
        return;
    }

    if (m_selected_option) {
        m_selected_option->set_selected(false);
    }

    if (option) {
        option->set_selected(true);
    }

    m_selected_option = option;
}

void SideBarOverlay::set_playlists(const std::vector<MusicItem>& playlists) {
    if (m_selected_option && m_playlists->contains(m_selected_option)) {
        m_selected_option = nullptr;
    }

    m_playlists->clear();
    for (const auto& item : playlists) {
        auto& option = m_playlists->add<OptionsWidget>(item, m_thumbnails);
        option.on_event(EventType::Click, [this, item](UiEvent&) {
            if (m_open_playlist) m_open_playlist(item);
        });
    }

    select(m_selected_id);
}
