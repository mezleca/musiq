#pragma once

#include "../session/ytb-music-session.hpp"
#include "../database/settings.hpp"

#include <SDL3/SDL_video.h>
#include <imgui-ui/runtime.hpp>
#include <imgui-ui/surface.hpp>
#include <future>

class SideBar;
class LoadingOverlay;

class MusiqApp {
public:
    explicit MusiqApp(SDL_Window* window, SDL_GLContext context);

    void initialize();

private:
    void sign_in();
    std::string open_sign_in();
    void authenticate(std::string token);
    void update_session();
    void update_profile();

    YoutubeMusicSession m_youtube_session;
    SideBar* m_sidebar = nullptr;
    LoadingOverlay* m_loading = nullptr;
    std::optional<MusiqSettings> m_settings;
    std::string m_session_token;

    ui::Runtime m_runtime;
    ui::Surface m_surface;

    std::future<std::optional<YoutubeMusicSession>> m_session;
};
