#pragma once

#include <arbor/widgets/widget.hpp>
#include <arbor/imgui/painter.hpp>

void paint_spinner(const ui::Painter& painter, ui::Rect rect, const ui::Color& color);

class SpinnerWidget final : public ui::Widget {
public:
    SpinnerWidget();

protected:
    void paint_content(const ui::PaintContext& context) override;
};
