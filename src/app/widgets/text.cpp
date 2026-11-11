#include "text.hpp"

#include <imgui-ui/surface.hpp>
#include <utility>

using namespace ui;

MusiqText::MusiqText(std::string text, FontWeight weight, float size)
    : TextWidget(std::move(text)), m_weight(weight), m_size(size) {}

void MusiqText::apply_theme_defaults(const Theme& theme) {
    TextWidget::apply_theme_defaults(theme);
    configure_all_styles([this](Style& style) { style.font_weight(m_weight).font_size(m_size); });
}
