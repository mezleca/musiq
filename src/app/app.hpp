#pragma once

#include "../youtube/session.hpp"

#include <SDL3/SDL_video.h>
#include <imgui-ui/runtime.hpp>
#include <imgui-ui/surface.hpp>
#include <future>

class SideBarOverlay;
class LoadingOverlay;
class SettingsOverlay;
class ChannelPickerOverlay;

class MusiqApp {
public:
    explicit MusiqApp(SDL_Window* window, SDL_GLContext context);

    void initialize();

private:
    void sign_in();
    void log_out();
    std::string open_sign_in();
    void authenticate(std::string token, std::string identity = {});
    void cancel_channel_selection();
    void update_session();
    void update_profile();

    YoutubeMusicSession m_youtube_session;
    SideBarOverlay* m_sidebar = nullptr;
    LoadingOverlay* m_loading = nullptr;
    SettingsOverlay* m_settings_overlay = nullptr;
    ChannelPickerOverlay* m_channel_picker = nullptr;
    std::string m_session_token;

    ui::Runtime m_runtime;
    ui::Surface m_surface;

    std::future<std::optional<YoutubeMusicSession>> m_session;
};
