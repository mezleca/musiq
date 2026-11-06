#pragma once

#include "../youtube/session.hpp"
#include "catalog-browser.hpp"
#include "resources/thumbnails.hpp"

#include <SDL3/SDL_video.h>
#include <imgui-ui/runtime.hpp>
#include <imgui-ui/surface.hpp>
#include <future>

class SideBarOverlay;
class LoadingOverlay;
class SettingsOverlay;
class ChannelPickerOverlay;
class HomeOverlay;
class SearchOverlay;
class SearchBarWidget;

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
    void navigate(std::string_view page);
    void submit_search(std::string query);
    void refresh_home();
    void refresh_search();
    void refresh_catalog();
    void update_catalog();

    YoutubeMusicSession m_youtube_session;
    SideBarOverlay* m_sidebar = nullptr;
    LoadingOverlay* m_loading = nullptr;
    SettingsOverlay* m_settings_overlay = nullptr;
    ChannelPickerOverlay* m_channel_picker = nullptr;
    std::string m_session_token;
    std::string m_page = "home";
    CatalogBrowser m_catalog;
    HomeOverlay* m_home = nullptr;
    SearchOverlay* m_search = nullptr;
    SearchBarWidget* m_search_bar = nullptr;
    ThumbnailCache m_thumbnails;

    ui::Runtime m_runtime;
    ui::Surface m_surface;

    std::future<std::optional<YoutubeMusicSession>> m_session;
};
