#pragma once

#include "text.hpp"
#include <arbor/layout/container.hpp>

class PlaceholderText final : public MusiqText {
public:
    explicit PlaceholderText(ui::FontWeight weight = ui::FontWeight::Regular, float size = 16.0f);

private:
    bool paint() override;
};

class PlaceholderCover final : public ui::Container {
public:
    explicit PlaceholderCover(float size);

private:
    void apply_theme_defaults(const ui::Theme& theme) override;
};
