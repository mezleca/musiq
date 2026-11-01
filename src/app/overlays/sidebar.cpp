#include "sidebar.hpp"
#include "../widgets/user-profile.hpp"

#include <utility>

using namespace ui;

SideBar::SideBar(std::function<void()> sign_in) : Container("sidebar", StackDirection::Vertical) {
    set_type_name("SideBar");
    set_size({px(280), grow()});
    set_spacing(16.0F);

    m_user_profile = &add<UserProfile>(std::move(sign_in));
    m_options = &add<Container>("musiq-options", StackDirection::Vertical);
    m_options->set_size({grow(), fit()});
    m_playlists = &add<Container>("user-playlists", StackDirection::Vertical);
    m_playlists->set_size({grow(), grow()});
    m_playlists->set_scrollable(true);
}

void SideBar::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    configure_all_styles([&theme](Style& style) { style.padding({16.0F}).background_color(theme.background_secondary_color); });
}
