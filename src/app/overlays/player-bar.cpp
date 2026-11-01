#include "player-bar.hpp"

using namespace ui;

PlayerBar::PlayerBar() : Container("player-bar", StackDirection::Horizontal) {
    set_type_name("PlayerBar");
    set_size({grow(), px(80)});
}

void PlayerBar::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    configure_all_styles([&theme](Style& style) {
        style.background_color(theme.background_tertiary_color).border_top(1.0F).border_color(theme.border_color);
    });
}
