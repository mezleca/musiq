#include "theme.hpp"

ui::Theme musiq_theme() {
    using namespace ui;
    Theme theme;
    theme.background_color = rgb(3, 3, 3);
    theme.background_secondary_color = rgb(15, 15, 15);
    theme.background_tertiary_color = rgb(8, 8, 8);
    theme.header_background_color = rgb(28, 28, 28);
    theme.text_secondary_color = rgb(170, 170, 170);
    theme.border_color = rgb(48, 48, 48);
    theme.header_border_color = theme.border_color;
    theme.accent_color = rgb(255, 0, 51);
    theme.accent_hover_color = rgb(255, 61, 90);
    theme.button_active_color = rgb(40, 40, 40);
    theme.controls.background_color = rgb(33, 33, 33);
    theme.controls.hover_color = rgb(45, 45, 45);
    theme.controls.active_color = rgb(55, 55, 55);
    theme.controls.border_color = theme.border_color;
    theme.controls.mark_color = theme.accent_color;
    theme.controls.rounding = 4.0f;
    theme.metrics.popup_rounding = 4.0f;
    theme.scrollbar.background_color = theme.transparent;
    theme.scrollbar.grab_color = rgb(85, 85, 85);
    theme.scrollbar.grab_hovered_color = rgb(115, 115, 115);
    theme.scrollbar.grab_active_color = rgb(145, 145, 145);
    theme.scrollbar.rounding = 3.0f;
    theme.scrollbar.grab_rounding = 3.0f;
    return theme;
}
