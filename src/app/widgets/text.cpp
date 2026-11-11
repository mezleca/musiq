#include "text.hpp"

#include <imgui-ui/surface.hpp>
#include <utility>

using namespace ui;

MusiqText::MusiqText(std::string text, FontWeight weight) : TextWidget(std::move(text)), m_weight(weight) {}

void MusiqText::apply_theme_defaults(const Theme& theme) {
    TextWidget::apply_theme_defaults(theme);
    configure_all_styles([this](Style& style) { style.font_weight(m_weight).font_size(16.0f); });
}
