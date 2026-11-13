#pragma once

#include <arbor/widgets/text.hpp>

class MusiqText : public ui::TextWidget {
public:
    explicit MusiqText(std::string text, ui::FontWeight weight = ui::FontWeight::Regular, float size = 16.0f);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    ui::FontWeight m_weight;
    float m_size;
};
