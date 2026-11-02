#include "spinner.hpp"

#include <cmath>
#include <numbers>

constexpr float pi = std::numbers::pi_v<float>;

SpinnerWidget::SpinnerWidget() : Widget("spinner", "SpinnerWidget", ui::InputMode::None) {
    set_size({ui::px(24), ui::px(24)});
}

void SpinnerWidget::paint_content(const ui::PaintContext& context) {
    const float angle = static_cast<float>(std::fmod(ImGui::GetTime(), 0.75) / 0.75) * 2.0f * pi;
    const ImVec2 center = {
        (context.content_rect.min.x + context.content_rect.max.x) * 0.5f,
        (context.content_rect.min.y + context.content_rect.max.y) * 0.5f
    };

    auto color = context.style.color().value.rgba();
    color.w *= context.painter.alpha();

    const ImU32 packed = ImGui::ColorConvertFloat4ToU32(color);
    auto& draw_list = context.painter.draw_list();
    draw_list.PathArcTo(center, 9.5f, angle, angle + 1.5f * pi, 32);
    draw_list.PathStroke(packed, 0, 3.0f);

    for (const float end : {angle, angle + 1.5f * pi}) {
        draw_list.AddCircleFilled({center.x + std::cos(end) * 9.5f, center.y + std::sin(end) * 9.5f}, 1.5f, packed);
    }
}
