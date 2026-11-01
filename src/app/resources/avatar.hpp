#pragma once

#include <string_view>

namespace ui {
    class Texture;
    class TextureRegistry;
} // namespace ui

ui::Texture* load_avatar(ui::TextureRegistry& textures, std::string_view id, std::string_view image);
