#include "button.hpp"

#include <imgui-ui/surface.hpp>
#include <utility>

Button::Button(std::string text, ui::LayoutSize size) : ButtonWidget(std::move(text), size) {}

void Button::apply_theme_defaults(const ui::Theme& theme) {
    ButtonWidget::apply_theme_defaults(theme);
    set_font(surface().get_font("Manrope Bold"), 14.0F);
}
