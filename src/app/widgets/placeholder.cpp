#include "placeholder.hpp"

#include <arbor/imgui/painter.hpp>
#include <arbor/surface.hpp>

using namespace ui;

PlaceholderText::PlaceholderText(FontWeight weight, float size) : MusiqText("", weight, size) {}

bool PlaceholderText::paint() {
    const auto paint = painter();
    const Rect rect = paint.to_logical(layout().visual_rect());
    const float height = rect.size().y * 0.5f;
    const Rect bar =
        Rect::from_position_size({rect.min.x, rect.min.y + (rect.size().y - height) * 0.5f}, {rect.size().x * 0.65f, height});

    paint.rect_filled(bar, surface().theme().background_tertiary_color, 3.0f);
    ImGui::Dummy(layout().size());

    return true;
}

PlaceholderCover::PlaceholderCover(float size) : Container("cover") {
    set_size({px(size), px(size)});
    set_input_mode(InputMode::None);
}

void PlaceholderCover::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);

    configure_all_styles([&theme](Style& style) {
        style.background_color(theme.background_tertiary_color).border_radius(theme.controls.rounding);
    });
}
