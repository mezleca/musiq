#include "system.hpp"

#include <memory>
#include <stdexcept>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

std::filesystem::path utils::get_app_dir() {
    const std::unique_ptr<char, decltype(&SDL_free)> directory(SDL_GetPrefPath(nullptr, "musiq"), SDL_free);
    if (!directory) {
        throw std::runtime_error(SDL_GetError());
    }

    return std::filesystem::path(reinterpret_cast<const char8_t*>(directory.get()));
}
