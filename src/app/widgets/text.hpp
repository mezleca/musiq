#pragma once

#include <imgui-ui/widgets/text.hpp>
#include "../resources/fonts.hpp"

class MusiqText : public ui::TextWidget {
public:
    explicit MusiqText(std::string text, TextWeight weight = TextWeight::Regular);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    TextWeight m_weight;
};
