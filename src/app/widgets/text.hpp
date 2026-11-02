#pragma once

#include <imgui-ui/widgets/text.hpp>

class MusiqText : public ui::TextWidget {
public:
    explicit MusiqText(std::string text, bool bold = false);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    bool m_bold;
};
