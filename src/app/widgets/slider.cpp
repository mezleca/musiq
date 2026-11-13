#include "slider.hpp"

#include <arbor/surface.hpp>
#include <algorithm>
#include <stdexcept>

using namespace ui;

MusiqSliderWidget::MusiqSliderWidget(double& value) : Widget("", "MusiqSliderWidget"), m_value(value) {
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

void MusiqSliderWidget::set_range(double minimum, double maximum) {
    if (minimum >= maximum) throw std::invalid_argument("slider minimum must be less than maximum");
    m_minimum = minimum;
    m_maximum = maximum;
    set_value(m_value);
}

void MusiqSliderWidget::set_value(double value) {
    value = std::clamp(value, m_minimum, m_maximum);
    if (value == m_value) return;
    m_value = value;
    if (!m_commit_on_release || !m_dragging) notify_change();
}

void MusiqSliderWidget::set_commit_on_release(bool enabled) {
    m_commit_on_release = enabled;
}

void MusiqSliderWidget::event(UiEvent& event) {
    if (event.type == EventType::Cancel) {
        if (m_dragging && m_commit_on_release) m_value = m_drag_start;
        m_dragging = false;
        return;
    }
    if (event.type == EventType::KeyDown && (event.key == ImGuiKey_LeftArrow || event.key == ImGuiKey_RightArrow)) {
        const double step = (m_maximum - m_minimum) / 100.0;
        set_value(m_value + (event.key == ImGuiKey_RightArrow ? step : -step));
    } else {
        // pointer capture keeps move and release events after the cursor leaves the track
        if (event.type == EventType::PointerDown && event.button == PointerButton::Left) {
            m_drag_start = m_value;
            m_dragging = true;
            surface().input_router().capture_pointer(*this);
            surface().input_router().set_focus(this);
        } else if (event.type == EventType::PointerUp && m_dragging) {
            m_dragging = false;
        } else if (event.type != EventType::PointerMove || !m_dragging) {
            return;
        }
        const auto rect = layout().visual_rect();
        if (rect.size().x > 0.0f) {
            const auto value = m_minimum + (event.position.x - rect.min.x) * (m_maximum - m_minimum) / rect.size().x;
            if (event.type == EventType::PointerUp && m_commit_on_release) {
                m_value = std::clamp(value, m_minimum, m_maximum);
                notify_change();
            } else {
                set_value(value);
            }
        }
    }

    event.prevent_default();
    event.stop_propagation();
    event.block_native_input();
}

void MusiqSliderWidget::paint_content(const PaintContext& context) {
    const bool active = input_state().hovered || m_dragging;
    const auto rect = context.content_rect;
    const float middle = (rect.min.y + rect.max.y) * 0.5f;
    const auto progress = std::clamp((m_value - m_minimum) / (m_maximum - m_minimum), 0.0, 1.0);
    const float end = rect.min.x + rect.size().x * static_cast<float>(progress);

    context.painter.rect_filled({{rect.min.x, middle - 2.0f}, {rect.max.x, middle + 2.0f}}, m_track, 2.0f);
    context.painter.rect_filled(
        {{rect.min.x, middle - 2.0f}, {end, middle + 2.0f}}, active ? m_active : context.style.color().value, 2.0f
    );

    if (active && enabled()) {
        context.painter.circle({end, middle}, 5.0f, context.style.color().value);
    }
}
