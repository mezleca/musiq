#pragma once

#include <imgui-ui/widgets/widget.hpp>
#include <string>

namespace ui {
    class ImageWidget;
} // namespace ui

class OptionsWidget final : public ui::Widget {
public:
    OptionsWidget(std::string id, std::string text, std::string icon_path, bool selected = false);
    void set_selected(bool selected);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void apply_selection();

    ui::ImageWidget* m_icon;
    std::string m_icon_path;
    bool m_selected;
};
