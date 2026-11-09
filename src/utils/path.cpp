#include "path.hpp"

#include <stdexcept>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>

std::filesystem::path utils::path::from_resources(const std::filesystem::path& relative_path) {
    static const auto root = [] {
        const char* base = SDL_GetBasePath();
        if (!base) {
            throw std::runtime_error(SDL_GetError());
        }

        const std::filesystem::path directory(reinterpret_cast<const char8_t*>(base));
        if (std::filesystem::is_directory(directory / "resources")) {
            return directory / "resources";
        }

        return (directory / MUSIQ_INSTALLED_RESOURCE_ROOT / "resources").lexically_normal();
    }();

    return root / relative_path;
}
