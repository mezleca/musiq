#include "fonts.hpp"
#include "../../utils/path.hpp"

#include <imgui-ui/resources/font-registry.hpp>
#include <imgui-ui/runtime.hpp>
#include <imgui-ui/surface.hpp>
#include <filesystem>
#include <stdexcept>
#include <string>

static std::filesystem::path font_path(std::string_view family, std::string_view weight) {
    const auto directory = utils::path::from_resources("fonts") / family;
    for (const auto* extension : {".ttf", ".otf"}) {
        const auto path = directory / (std::string(weight) + extension);
        if (std::filesystem::exists(path)) return path;
    }
    return {};
}

MusiqFonts& MusiqFonts::instance() {
    static MusiqFonts fonts;
    return fonts;
}

void MusiqFonts::set_family(ui::Surface& surface, std::string_view family) {
    auto& registry = surface.runtime().fonts();
    constexpr std::array weights{"regular", "semibold", "bold"};
    std::array<ImFont*, 3> fonts{};
    ui::Font* primary = nullptr;

    for (std::size_t index = 0; index < weights.size(); ++index) {
        const auto path = font_path(family, weights[index]);
        if (path.empty()) {
            if (index == 1) {
                fonts[index] = fonts[0];
                continue;
            }
            throw std::runtime_error("missing font: " + std::string(family) + "/" + weights[index]);
        }

        const auto id = std::string(family) + ":" + weights[index];
        auto* font = registry.add(id, path);
        fonts[index] = surface.get_font(id);
        if (index == 0) primary = font;

        // the base font supplies its own glyphs first, CJK fills only missing characters
        if (family != "noto-sans-cjk-jp") {
            ImFontConfig fallback;
            fallback.MergeMode = true;
            fallback.DstFont = fonts[index];
            const auto fallback_id = id + ":cjk";
            registry.add(fallback_id, font_path("noto-sans-cjk-jp", index == 2 ? "bold" : "regular"), fallback);
            surface.get_font(fallback_id);
        }
    }

    m_fonts = fonts;
    surface.set_primary_font(primary);
    // reapply widget defaults so cached font pointers and text measurements use the new family
    surface.set_theme(surface.theme());
}

ImFont* MusiqFonts::get(TextWeight weight) const {
    auto* font = m_fonts[static_cast<std::size_t>(weight)];
    if (!font) throw std::logic_error("fonts must be loaded before use");
    return font;
}
