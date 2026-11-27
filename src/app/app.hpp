#pragma once

#include "../youtube/session.hpp"
#include "../youtube/catalog-browser.hpp"
#include "../utils/navigation-history.hpp"
#include "../session/session.hpp"
#include "resources/thumbnails.hpp"
#include "overlays/channel-picker.hpp"
#include "overlays/playlist-editor.hpp"
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
#include "../utils/task-queue.hpp"
#include <SDL3/SDL_video.h>
#include <arbor/runtime.hpp>
#include <arbor/surface.hpp>
#include <functional>
#include <filesystem>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

struct PlaylistAddState;

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
    void authenticate(std::string token, std::string identity = {});
    void choose_channel();
    void load_avatar();
    void cancel_channel_selection();
    void update_session();
    void session_changed(SessionAction action, YoutubeResult<void> result);
    void update_profile();
    void refresh_profile();

    void navigate(std::string_view page);
    void show_page(PlaylistLoad playlist_load = PlaylistLoad::Normal);
    void prune_pages();
    void submit_search(std::string query);

    void update_catalog();
    CatalogActions catalog_actions();
    MediaMenu media_menu_handler();
    ui::ContextMenuItems media_menu(const MusicItem& item, MediaMenuContext context);
    void update_playback();
    void update_tracks(ui::Node& root);
    void open_playlist(MusicItem item, PlaylistLoad load = PlaylistLoad::Normal);
    void show_on_playlist(const std::string& source, const MusicItem& track);
    void save_playlist(PlaylistDraft draft);
    void create_playlist(PlaylistDraft&& draft);
    void edit_playlist(PlaylistDraft&& draft);
    void delete_playlist(std::string id);
    void unsave_playlist(std::string id);
    void add_playlist_track(std::string playlist_id, std::string video_id, std::shared_ptr<PlaylistAddState> state);
    void remove_playlist_track(std::string playlist_id, MusicPlaylistRemoveRequest::Entry entry);
    void refresh_playlist(const std::string& playlist_id);
    void update_shortcuts();

    MusiqSession m_session;
    SideBarOverlay* m_sidebar = nullptr;
    UserProfileWidget* m_profile = nullptr;
    LoadingOverlay* m_loading = nullptr;
    SettingsOverlay* m_settings_overlay = nullptr;
    ChannelPickerOverlay* m_channel_picker = nullptr;
    PlaylistEditorOverlay* m_playlist_editor = nullptr;
    std::unordered_map<std::string, std::weak_ptr<PlaylistAddState>> m_playlist_additions;
    MediaContextMenu* m_context_menu = nullptr;
    NavigationHistory m_history;
    TaskQueue& m_tasks = TaskQueue::instance();
    CatalogBrowser m_catalog;
    CatalogResultsWidget* m_home = nullptr;
    std::unordered_map<std::string, CatalogResultsWidget*> m_searches;
    std::unordered_map<std::string, PlaylistOverlay*> m_playlists;
    std::unordered_map<std::string, MusicItem> m_playlist_items;
    std::vector<NavigationRoute> m_page_order;
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

    ui::Runtime m_runtime;
    ui::Surface m_surface;
    bool m_fonts_changed = false;
};
