#pragma once

#include "../youtube/session.hpp"
#include "../youtube/catalog-browser.hpp"
#include "page-history.hpp"
#include "resources/thumbnails.hpp"
#include "overlays/channel-picker.hpp"
#include "overlays/loading.hpp"
#include "overlays/player-bar.hpp"
#include "overlays/playlist.hpp"
#include "overlays/queue.hpp"
#include "overlays/settings.hpp"
#include "overlays/sidebar.hpp"
#include "widgets/button.hpp"
#include "widgets/catalog-results.hpp"
#include "widgets/search-bar.hpp"
#include "widgets/user-profile.hpp"
#include "../playback/controller.hpp"
#include "../playback/mpv-player.hpp"
#include "../utils/thread-pool.hpp"

#include <SDL3/SDL_video.h>
#include <imgui-ui/runtime.hpp>
#include <imgui-ui/surface.hpp>
#include <future>
#include <string_view>
#include <unordered_map>

class MusiqApp {
public:
    explicit MusiqApp(SDL_Window* window, SDL_GLContext context);

    void initialize();

private:
    void build_navigation(ui::Container& content);
    void build_pages(ui::Container& content);
    void build_player();
    void sign_in();
    void log_out();
    void clear_session();
    void clear_browser_cookies();
    std::string open_sign_in();
    void authenticate(std::string token, std::string identity = {});
    void cancel_channel_selection();
    void update_session();
    void update_shortcuts();
    void update_profile();
    void navigate(std::string_view page);
    void show_page();
    void submit_search(std::string query);
    void refresh_page();
    void update_catalog();
    CatalogActions catalog_actions();
    void update_playback();
    void open_playlist(MusicItem item);

    YoutubeMusicSession m_youtube_session;
    SideBarOverlay* m_sidebar = nullptr;
    UserProfileWidget* m_profile = nullptr;
    LoadingOverlay* m_loading = nullptr;
    SettingsOverlay* m_settings_overlay = nullptr;
    ChannelPickerOverlay* m_channel_picker = nullptr;
    std::string m_session_token;
    PageHistory m_history;
    ThreadPool m_workers;
    CatalogBrowser m_catalog;
    CatalogResultsWidget* m_home = nullptr;
    std::unordered_map<std::string, CatalogResultsWidget*> m_searches;
    std::unordered_map<std::string, PlaylistOverlay*> m_playlists;
    ui::Container* m_pages = nullptr;
    ui::Container* m_current_page = nullptr;
    MusiqButtonWidget* m_back = nullptr;
    MusiqButtonWidget* m_forward = nullptr;
    SearchBarWidget* m_search_bar = nullptr;
    ThumbnailCache m_thumbnails;
    MpvPlayer m_player;
    PlaybackController m_playback;
    PlayerBarOverlay* m_player_bar = nullptr;
    QueueOverlay* m_queue_overlay = nullptr;
    std::uint64_t m_queue_revision = 0;

    ui::Runtime m_runtime;
    ui::Surface m_surface;

    std::future<std::optional<YoutubeMusicSession>> m_session;
};
