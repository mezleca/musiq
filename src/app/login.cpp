#include "login.hpp"
#include "../constants.hpp"
#include "../youtube/session.hpp"
#include "../utils/log.hpp"

#include <arbor/surface.hpp>
#include <SDL3/SDL.h>
#include <exception>
#include <stdexcept>
#include <utility>

// webview pulls in x11 macros that conflict with arbor enum names
#include "../webview/webview.hpp"

class BrowserGraphicsScope final {
public:
    explicit BrowserGraphicsScope(ui::Surface& surface)
        : m_surface(surface), m_window(SDL_GL_GetCurrentWindow()), m_context(SDL_GL_GetCurrentContext()) {
        // gtk changes egl state without updating sdl's cached current context
        if (!SDL_GL_MakeCurrent(nullptr, nullptr)) {
            m_surface.exit();
            throw std::runtime_error(SDL_GetError());
        }
    }

    ~BrowserGraphicsScope() {
        if (!SDL_GL_MakeCurrent(m_window, m_context)) {
            LOG_ERROR("[login] could not restore the graphics context: {}", SDL_GetError());
            m_surface.exit();
        }
    }

private:
    ui::Surface& m_surface;
    SDL_Window* m_window;
    SDL_GLContext m_context;
};

std::string open_login(ui::Surface& surface) {
    try {
        std::string token;
        bool reading = false;

        {
            const BrowserGraphicsScope graphics(surface);
            auto window = WebView::create("Sign in");
            window->run(MUSIC_LOGIN_URL, [&] {
                if (reading || !token.empty() || !window->url().starts_with(MUSIC_HOME_URL)) return;

                reading = true;
                window->cookies(MUSIC_HOME_URL, [&](std::string cookies) {
                    reading = false;
                    if (!YoutubeMusicSession::has_session_cookie(cookies)) return;

                    token = std::move(cookies);
                    window->close();
                });
            });
        }

        // discard clicks queued while the modal browser was handling input
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP);

        return surface.is_done() ? std::string{} : token;
    } catch (const std::exception& error) {
        LOG_ERROR("[login] failed to open sign-in window: {}", error.what());
        return {};
    }
}

void clear_browser_cookies(ui::Surface& surface) {
    {
        const BrowserGraphicsScope graphics(surface);
        WebView::create("Clear cookies")->clear_cookies();
    }

    if (surface.is_done()) {
        throw std::runtime_error("could not restore the graphics context");
    }

    LOG_INFO("[login] browser cookies cleared");
}
