#pragma once

#include <imgui-ui/widgets/button.hpp>

namespace ui {
    class ImageWidget;
}

class MusiqButtonWidget : public ui::ButtonWidget {
public:
    explicit MusiqButtonWidget(std::string text, ui::LayoutSize size = {ui::fit(), ui::fit()});

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
};

class MusiqIconButtonWidget : public MusiqButtonWidget {
public:
    explicit MusiqIconButtonWidget(std::string icon_path, float size = 32.0f);

    void set_icon(std::string path);
    void set_selected(bool value);

    bool selected() const {
        return m_selected;
    }

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    std::string m_path;
    ui::ImageWidget* m_icon;
    bool m_selected = false;
};
