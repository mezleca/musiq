#include "avatar.hpp"
#include "../../utils/log.hpp"
#include "../../utils/system.hpp"

#include <imgui-ui/resources/texture-registry.hpp>
#include <fstream>
#include <string>
#include <utility>

ui::Texture* load_avatar(ui::TextureRegistry& textures, std::string_view id, std::string_view image) {
    if (image.empty()) {
        return nullptr;
    }

    std::string key = "profile:";
    key += id;

    if (auto* texture = textures.find(key)) {
        return texture;
    }

    try {
        const auto path = utils::get_app_dir() / "avatar.image";

        std::ofstream file(path, std::ios::binary);
        file.write(image.data(), static_cast<std::streamsize>(image.size()));
        file.close();

        if (!file) {
            LOG_WARN("[ui] could not write the avatar cache");
            return nullptr;
        }
        return textures.add(std::move(key), path);
    } catch (const std::exception& error) {
        LOG_WARN("[ui] could not load the avatar: {}", error.what());
        return nullptr;
    }
}
