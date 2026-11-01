#pragma once

#include <imgui-ui/widgets/button.hpp>

class Button : public ui::ButtonWidget {
public:
    explicit Button(std::string text, ui::LayoutSize size = {ui::fit(), ui::fit()});

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
};
