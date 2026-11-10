#include "text.hpp"

#include <imgui-ui/surface.hpp>
#include <utility>

using namespace ui;

MusiqText::MusiqText(std::string text, TextWeight weight) : TextWidget(std::move(text)), m_weight(weight) {}

void MusiqText::apply_theme_defaults(const Theme& theme) {
    TextWidget::apply_theme_defaults(theme);
    set_font(MusiqFonts::instance().get(m_weight), 16.0f);
}
