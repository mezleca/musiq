#pragma once

#include <array>
#include <string_view>

struct ImFont;
namespace ui {
    class Surface;
}

enum class TextWeight {
    Regular,
    Semibold,
    Bold
};

class MusiqFonts {
public:
    static MusiqFonts& instance();

    MusiqFonts(const MusiqFonts&) = delete;
    MusiqFonts& operator=(const MusiqFonts&) = delete;
    MusiqFonts(MusiqFonts&&) = delete;
    MusiqFonts& operator=(MusiqFonts&&) = delete;

    void set_family(ui::Surface& surface, std::string_view family);
    ImFont* get(TextWeight weight = TextWeight::Regular) const;

private:
    MusiqFonts() = default;
    std::array<ImFont*, 3> m_fonts{};
};
