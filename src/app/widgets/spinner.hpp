#pragma once

#include <imgui-ui/widgets/widget.hpp>

class SpinnerWidget final : public ui::Widget {
public:
    SpinnerWidget();

protected:
    void paint_content(const ui::PaintContext& context) override;
};
