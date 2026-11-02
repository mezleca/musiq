#include "button.hpp"

#include <imgui-ui/surface.hpp>
#include <utility>

MusiqButtonWidget::MusiqButtonWidget(std::string text, ui::LayoutSize size) : ui::ButtonWidget(std::move(text), size) {
    set_type_name("MusiqButtonWidget");
}

void MusiqButtonWidget::apply_theme_defaults(const ui::Theme& theme) {
    ui::ButtonWidget::apply_theme_defaults(theme);

    set_font(surface().get_font("Manrope Bold"), 14.0f);
}
