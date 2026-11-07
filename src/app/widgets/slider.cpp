#include "slider.hpp"

#include <imgui-ui/surface.hpp>
#include <algorithm>

using namespace ui;

MusiqSliderWidget::MusiqSliderWidget(double& percent) : Widget("", "MusiqSliderWidget"), m_percent(percent) {
    set_size({grow(), px(20)});
}

void MusiqSliderWidget::apply_theme_defaults(const Theme& theme) {
    Widget::apply_theme_defaults(theme);

    m_track = theme.border_color;
    m_active = theme.accent_color;

    configure_all_styles([&theme](Style& style) {
        style.background_color(theme.transparent)
            .color(theme.text_color)
            .padding({})
            .border_thickness(0.0f)
            .cursor(ImGuiMouseCursor_Hand);
    });
}

void MusiqSliderWidget::set_value(double percent) {
    percent = std::clamp(percent, 0.0, 100.0);
    if (percent == m_percent) return;
    m_percent = percent;
    notify_change();
}

void MusiqSliderWidget::event(UiEvent& event) {
    if (event.type == EventType::Cancel) {
        m_dragging = false;
        return;
    }
    if (event.type == EventType::KeyDown && (event.key == Key::Left || event.key == Key::Right)) {
        set_value(m_percent + (event.key == Key::Right ? 1 : -1));
    } else {
        // pointer capture keeps move and release events after the cursor leaves the track
        if (event.type == EventType::PointerDown && event.button == PointerButton::Left) {
            m_dragging = true;
            surface().input_router().capture_pointer(*this);
            surface().input_router().set_focus(this);
        } else if (event.type == EventType::PointerUp && m_dragging) {
            m_dragging = false;
        } else if (event.type != EventType::PointerMove || !m_dragging) {
            return;
        }
        const auto rect = layout().visual_rect();
        set_value((event.position.x - rect.min.x) * 100.0 / rect.size().x);
    }

    event.prevent_default();
    event.stop_propagation();
    event.block_native_input();
}

void MusiqSliderWidget::paint_content(const PaintContext& context) {
    const bool active = input_state().hovered || m_dragging;
    const auto rect = context.content_rect;
    const float middle = (rect.min.y + rect.max.y) * 0.5f;
    const float end = rect.min.x + rect.size().x * static_cast<float>(std::clamp(m_percent, 0.0, 100.0) / 100.0);

    context.painter.rect_filled({{rect.min.x, middle - 2.0f}, {rect.max.x, middle + 2.0f}}, m_track, 2.0f);
    context.painter.rect_filled(
        {{rect.min.x, middle - 2.0f}, {end, middle + 2.0f}}, active ? m_active : context.style.color().value, 2.0f
    );

    if (active && enabled()) {
        context.painter.circle({end, middle}, 5.0f, context.style.color().value);
    }
}
