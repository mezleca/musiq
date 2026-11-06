#include "avatar.hpp"

#include <imgui-ui/resources/texture-registry.hpp>
#include <string>
#include <utility>

ui::Texture* load_avatar(ui::TextureRegistry& textures, std::string_view id, std::string_view image) {
    if (image.empty()) {
        return nullptr;
    }

    std::string key = "profile:";
    key += id;

    return textures.add(std::move(key), image);
}
