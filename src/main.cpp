#include "app/app.hpp"
#include "database/settings.hpp"
#include "utils/system.hpp"
#include "utils/log.hpp"

#include <SDL3/SDL.h>
#include <exception>

int main() {
    SDL_SetHint(SDL_HINT_APP_ID, "org.mezleca.musiq");
#if defined(__linux__)
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("musiq", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        SDL_Quit();
        return 1;
    }

    SDL_Surface* icon = SDL_LoadPNG("resources/icons/app/musiq.png");
    if (icon) {
        if (!SDL_SetWindowIcon(window, icon)) {
            LOG_WARN("[window] could not set icon: {}", SDL_GetError());
        }
        SDL_DestroySurface(icon);
    } else {
        LOG_WARN("[window] could not load icon: {}", SDL_GetError());
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);

    // initialize main app loop
    {
        try {
            MusiqSettings::initialize((utils::get_app_dir() / "settings.sqlite").string());
            MusiqApp app(window, context);
            app.initialize();
        } catch (std::exception& error) {
            LOG_ERROR("failed to initialize: {}", error.what());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
