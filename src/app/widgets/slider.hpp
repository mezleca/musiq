#pragma once

#include <arbor/widgets/widget.hpp>

class MusiqSliderWidget final : public ui::Widget {
public:
    explicit MusiqSliderWidget(double& value);
    void set_range(double minimum, double maximum);
    void set_commit_on_release(bool enabled);
    bool dragging() const {
        return m_dragging;
    }

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
    void paint_content(const ui::PaintContext& context) override;
    void event(ui::UiEvent& event) override;

private:
    void set_value(double value);
    double& m_value;
    double m_minimum = 0;
    double m_maximum = 100;
    double m_drag_start = 0;
    ui::Color m_track;
    ui::Color m_active;
    bool m_dragging = false;
    bool m_commit_on_release = false;
};
