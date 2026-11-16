#include "spinner.hpp"

#include <cmath>
#include <numbers>

using namespace ui;

constexpr float pi = std::numbers::pi_v<float>;

void paint_spinner(const Painter& painter, Rect rect, const Color& source) {
    const float angle = static_cast<float>(std::fmod(ImGui::GetTime(), 0.75) / 0.75) * 2.0f * pi;
    const ImVec2 center = painter.to_screen({(rect.min.x + rect.max.x) * 0.5f, (rect.min.y + rect.max.y) * 0.5f});
    const float scale = dpi_scale();
    const float radius = 9.5f * scale;

    auto color = source.rgba();
    color.w *= painter.alpha();

    const ImU32 packed = ImGui::ColorConvertFloat4ToU32(color);
    auto& draw_list = painter.draw_list();
    draw_list.PathArcTo(center, radius, angle, angle + 1.5f * pi, 32);
    draw_list.PathStroke(packed, 0, 3.0f * scale);

    for (const float end : {angle, angle + 1.5f * pi}) {
        draw_list.AddCircleFilled({center.x + std::cos(end) * radius, center.y + std::sin(end) * radius}, 1.5f * scale, packed);
    }
}

SpinnerWidget::SpinnerWidget() : Widget("spinner", "SpinnerWidget", InputMode::None) {
    set_size({px(24), px(24)});
}

void SpinnerWidget::paint_content(const PaintContext& context) {
    paint_spinner(context.painter, context.content_rect, context.style.color().value);
}
