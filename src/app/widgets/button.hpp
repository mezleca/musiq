#pragma once

#include <imgui-ui/widgets/button.hpp>

class MusiqButtonWidget : public ui::ButtonWidget {
public:
    explicit MusiqButtonWidget(std::string text, ui::LayoutSize size = {ui::fit(), ui::fit()});

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
};
