#pragma once

#include <imgui-ui/layout/container.hpp>

class PlayerBar final : public ui::Container {
public:
    PlayerBar();

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
};
