#include "text.hpp"

#include <imgui-ui/surface.hpp>
#include <utility>

using namespace ui;

MusiqText::MusiqText(std::string text, bool bold) : TextWidget(std::move(text)), m_bold(bold) {}

void MusiqText::apply_theme_defaults(const Theme& theme) {
    TextWidget::apply_theme_defaults(theme);
    set_font(m_bold ? surface().get_font("Manrope Bold") : surface().get_primary_font(), 16.0f);
}
