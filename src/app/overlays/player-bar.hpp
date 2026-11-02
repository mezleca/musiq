#pragma once

#include <imgui-ui/layout/container.hpp>

class PlayerBarOverlay final : public ui::Container {
public:
    PlayerBarOverlay();

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
};
