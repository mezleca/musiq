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
#include <arbor/runtime.hpp>
#include <arbor/surface.hpp>
#include <functional>
#include <future>
#include <string_view>
#include <unordered_map>
#include <utility>

enum class SessionAction {
    SignIn,
    Refresh,
    Channels
};

class MusiqApp {
public:
    explicit MusiqApp(SDL_Window* window, SDL_GLContext context);

    void initialize();

private:
    void apply_fonts();
    void build_navigation(ui::Container& content);
    void build_pages(ui::Container& content);
    void build_player();
    void sign_in();
    void log_out();
    void clear_session();
    void clear_browser_cookies();
    std::string open_sign_in();
    void run_session(SessionAction action, std::function<YoutubeResult<void>(YoutubeMusicSession&)> task);
    void authenticate(std::string token, std::string identity = {});
    void refresh_session();
    void choose_channel();
    void load_avatar();
    void cancel_channel_selection();
    void update_session();
    void update_shortcuts();
    void update_profile();
    void refresh_profile();
    void navigate(std::string_view page);
    void show_page();
    void submit_search(std::string query);
    void refresh_page();
    void update_catalog();
    CatalogActions catalog_actions();
    void update_playback();
    void update_tracks(ui::Node& root);
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
    MusiqText* m_navigation_title = nullptr;
    ThumbnailCache m_thumbnails;
    MpvPlayer m_player;
    PlaybackController m_playback;
    PlayerBarOverlay* m_player_bar = nullptr;
    QueueOverlay* m_queue_overlay = nullptr;
    std::uint64_t m_queue_revision = 0;

    ui::Runtime m_runtime;
    ui::Surface m_surface;
    bool m_fonts_changed = false;

    SessionAction m_session_action = SessionAction::SignIn;
    std::future<YoutubeResult<YoutubeMusicSession>> m_session;
    std::future<std::pair<std::string, std::string>> m_avatar;
};
