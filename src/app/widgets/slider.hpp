#pragma once

#include <imgui-ui/widgets/widget.hpp>

class MusiqSliderWidget final : public ui::Widget {
public:
    explicit MusiqSliderWidget(double& percent);
    bool dragging() const {
        return m_dragging;
    }

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
    void paint_content(const ui::PaintContext& context) override;
    void event(ui::UiEvent& event) override;

private:
    void set_value(double percent);
    double& m_percent;
    ui::Color m_track;
    ui::Color m_active;
    bool m_dragging = false;
};
