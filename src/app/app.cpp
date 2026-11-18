#include "app.hpp"
#include "theme.hpp"
#include "widgets/text.hpp"
#include "../utils/media.hpp"
#include "../database/cache.hpp"
#include "../database/settings.hpp"
#include "../utils/json.hpp"
#include "../utils/log.hpp"
#include "login.hpp"
#include "../youtube/client.hpp"
#include "../youtube/playlists.hpp"

#include <arbor/backends/opengl/texture-loader.hpp>
#include <arbor/backends/sdl/backend.hpp>
#include <arbor/layout/container.hpp>
#include <algorithm>
#include <memory>
#include <utility>
#include <cstdlib>

static constexpr size_t MAX_PAGES = 8;

using namespace ui;
using namespace utils::media;

static RuntimeConfig make_runtime_config() {
    return {
        .performance_directory = {},
        .texture_loader = std::make_unique<OpenGLTextureLoader>(),
        .texture_cache = {.cpu_budget_bytes = 128 * 1024 * 1024, .gpu_budget_bytes = 128 * 1024 * 1024},
    };
}

static SurfaceConfig make_surface_config(SDL_Window* window, SDL_GLContext context) {
    const auto* debugger = std::getenv("MUSIQ_ENABLE_DEBUGGER");
    bool enable_debugger = debugger && std::string_view(debugger) == "1";
    return {
        .backend = std::make_unique<SdlBackend>(window, context),
        .enable_debugger = enable_debugger,
        .file_dialog_backend = nullptr,
    };
}

MusiqApp::MusiqApp(SDL_Window* window, SDL_GLContext context)
    : m_playback(m_player), m_runtime(make_runtime_config()), m_surface(m_runtime, make_surface_config(window, context)) {
    m_surface.set_theme(musiq_theme());
    m_surface.set_scale(MusiqSettings::instance().interface_scale());
    apply_fonts();

    auto& main = m_surface.root().add<Container>("main", StackDirection::Horizontal);
    main.set_size({grow(), grow()});

    m_sidebar = &main.add<SideBarOverlay>(
        m_thumbnails, [this](std::string_view option) { navigate(option); },
        [this](MusicItem item) { open_playlist(std::move(item)); },
        [this] {
            if (m_playlist_editor) m_playlist_editor->show_create();
        }
    );

    auto& content = main.add<Container>("content", StackDirection::Vertical);
    content.set_size({grow(), grow()});

    build_navigation(content);
    build_pages(content);
    build_player();
    m_playback.on_queue_change([this] {
        m_queue_overlay->refresh(true);
        update_tracks(m_surface.root());
    });

    m_loading = &m_surface.root().add<LoadingOverlay>();
    m_channel_picker = &m_surface.root().add<ChannelPickerOverlay>(*m_profile, [this](std::string identity) {
        // channel selection reuses the credentials already owned by the authenticated session
        authenticate({}, std::move(identity));
    });

    m_playlist_editor =
        &m_surface.root().add<PlaylistEditorOverlay>([this](PlaylistDraft draft) { save_playlist(std::move(draft)); });
}

void MusiqApp::apply_fonts() {
    FontRequest fonts;
    auto& settings = MusiqSettings::instance();

    const auto main_font = settings.main_font();
    const auto fallback_font = settings.fallback_font();

    if (!main_font.empty()) {
        fonts.main = FontSelection::family(main_font);
    }

    if (!fallback_font.empty()) {
        fonts.fallback = FontSelection::family(fallback_font);
    }

    m_surface.set_primary_font(m_runtime.fonts().set("interface", fonts));
    m_surface.set_theme(m_surface.theme());
}

void MusiqApp::build_navigation(Container& content) {
    auto& navigation = content.add<Container>("navigation", StackDirection::Horizontal);

    navigation.set_size({grow(), px(80)});
    navigation.set_content_alignment(Anchor::CenterLeft);
    navigation.set_spacing(8.0f);
    navigation.configure_all_styles([](Style& style) { style.padding({16.0f}); });

    m_back = &navigation.add<MusiqIconButtonWidget>("icons/chevron-left.svg", 36.0f);
    m_back->set_enabled(false);
    m_back->on_click([this] {
        if (m_history.back()) {
            show_page();
        }
    });

    m_forward = &navigation.add<MusiqIconButtonWidget>("icons/chevron-right.svg", 36.0f);
    m_forward->set_enabled(false);
    m_forward->on_click([this] {
        if (m_history.forward()) {
            show_page();
        }
    });

    m_search_bar = &navigation.add<SearchBarWidget>(
        [this](std::string query) { submit_search(std::move(query)); },
        [this](std::string query) { m_catalog.suggest(std::move(query)); }
    );

    m_navigation_title = &navigation.add<MusiqText>("", FontWeight::Bold, 22.0f);
    m_navigation_title->set_size({{px(0), px(800)}, fit()});
    m_navigation_title->set_alignment({0.0f, 0.5f});
    m_navigation_title->set_visible(false);

    navigation.add<Container>("profile-spacer").set_size({grow(), px(1)});
    m_profile = &navigation.add<UserProfileWidget>([this] { sign_in(); }, [this] { choose_channel(); });
}

void MusiqApp::build_pages(Container& content) {
    m_pages = &content.add<Container>("pages", StackDirection::Vertical);
    m_pages->set_size({grow(), grow()});

    m_home = &m_pages->add<CatalogResultsWidget>("home", "Home", m_thumbnails);
    m_home->set_actions(catalog_actions());
    m_current_page = m_home;

    m_settings_overlay = &m_pages->add<SettingsOverlay>(
        m_runtime.fonts(), [this] { log_out(); },
        [this](int volume) {
            m_player_bar->set_volume(volume);
            m_player.set_volume(volume);
        },
        [this] { clear_browser_cookies(m_surface); }, [this] { m_fonts_changed = true; }, m_playback.queue()
    );
}

void MusiqApp::build_player() {
    m_playback.on_volume([this](int volume) { m_settings_overlay->set_volume(volume); });

    auto& player = m_surface.root().add<Container>("player", StackDirection::Vertical);
    player.set_size({grow(), fit()});
    m_player_bar = &player.add<PlayerBarOverlay>(m_thumbnails, m_player, m_playback, [this](int volume) {
        m_settings_overlay->set_volume(volume);
    });

    // keep the floating control outside the player bar's border clip
    player.add<QueueButtonWidget>().on_event(EventType::Click, [this](UiEvent& event) {
        m_queue_overlay->show();
        event.stop_propagation();
    });

    m_queue_overlay = &m_surface.root().add<QueueOverlay>(m_playback, m_thumbnails);
}

void MusiqApp::navigate(std::string_view page) {
    m_history.visit({std::string(page), page == "search" ? m_catalog.search_state().query : std::string{}});
    show_page();
}

void MusiqApp::show_page() {
    const auto& route = m_history.current();
    m_sidebar->select(route.page == "playlist" ? route.query : route.page);
    m_back->set_enabled(m_history.can_back());
    m_forward->set_enabled(m_history.can_forward());

    const bool settings_open = route.page == "settings";
    m_search_bar->set_visible(!settings_open);
    m_navigation_title->set_text(settings_open ? "Settings" : "");
    m_navigation_title->set_visible(settings_open);
    m_search_bar->set_query(route.page == "search" ? route.query : std::string{});
    m_catalog.suggest({});

    Container* next = nullptr;
    if (route.page == "home") {
        m_catalog.home();
        m_home->show(m_catalog.home_state());
        next = m_home;
    } else if (route.page == "search") {
        m_catalog.search(route.query);
        auto [entry, created] = m_searches.try_emplace(route.query, nullptr);
        if (created) {
            const auto title = route.query.empty() ? "Search" : "Results for: " + route.query;
            entry->second = &m_pages->add<CatalogResultsWidget>("search-" + route.query, title, m_thumbnails);
            entry->second->set_actions(catalog_actions());
        }

        entry->second->show(m_catalog.search_state());
        next = entry->second;
    } else if (route.page == "playlist") {
        auto [entry, created] = m_playlists.try_emplace(route.query, nullptr);
        if (created) {
            entry->second = &m_pages->add<PlaylistOverlay>(
                m_playlist_items.at(route.query), m_thumbnails, m_playback, [this](const MusicPlaylist& playlist) {
                    if (m_playlist_editor) m_playlist_editor->show_edit(playlist);
                }
            );
        }
        auto* view = entry->second;
        view->load(m_session.youtube());
        next = view;
    } else {
        next = m_settings_overlay;
    }

    // keep input and measure state when the same page is shown again
    if (next != m_current_page) {
        m_current_page->set_visible(false);
        next->set_visible(true);
        m_current_page = next;
    }

    update_tracks(*m_current_page);

    if (route.page == "search" || route.page == "playlist") {
        std::erase(m_page_order, route);
        m_page_order.push_back(route);
    }
    prune_pages();
}

void MusiqApp::prune_pages() {
    while (m_page_order.size() > MAX_PAGES) {
        const NavigationRoute oldest = std::move(m_page_order.front());
        m_page_order.erase(m_page_order.begin());
        if (oldest.page == "search") {
            m_pages->remove(*m_searches.at(oldest.query));
            m_searches.erase(oldest.query);
        } else {
            // history keeps playlist ids, so recreating an evicted view still needs its source item
            m_pages->remove(*m_playlists.at(oldest.query));
            m_playlists.erase(oldest.query);
        }
    }
}

void MusiqApp::submit_search(std::string query) {
    m_history.visit({"search", std::move(query)});
    show_page();
}

void MusiqApp::refresh_page() {
    const auto& route = m_history.current();

    if (route.page == "home") {
        m_catalog.home(true);
    } else if (route.page == "search") {
        m_catalog.search(route.query, true);
    } else if (route.page == "playlist") {
        m_playlists.at(route.query)->load(m_session.youtube(), true);
    }

    show_page();
}

void MusiqApp::update_catalog() {
    const auto changed = m_catalog.update();
    if (changed.library) m_sidebar->set_playlists(m_catalog.library_playlists());

    if (changed.home) {
        m_home->show(m_catalog.home_state());
        update_tracks(*m_home);
    }

    if (!changed.home && m_current_page == m_home && m_home->near_bottom() && m_catalog.more_home()) {
        m_home->show(m_catalog.home_state());
    }

    for (const auto& query : changed.searches) {
        const auto view = m_searches.find(query);
        if (view != m_searches.end()) {
            view->second->show(m_catalog.search_state(query));
            update_tracks(*view->second);
        }
    }

    if (changed.suggestions) {
        m_search_bar->set_suggestions(m_catalog.suggestions());
    }
}

CatalogActions MusiqApp::catalog_actions() {
    return {
        [this](MusicItem item) {
            if (is_playlist_card(item)) {
                open_playlist(std::move(item));
            } else {
                m_playback.play(std::move(item));
            }
        },
        [this](MusicItem item) { m_playback.queue().play_next(std::move(item)); },
        [this](MusicItem item) { m_playback.queue().append(std::move(item)); }
    };
}

void MusiqApp::open_playlist(MusicItem item) {
    const auto id = item.endpoint.browse_id;
    m_playlist_items.insert_or_assign(id, std::move(item));

    m_history.visit({"playlist", id});
    show_page();
}

void MusiqApp::save_playlist(PlaylistDraft draft) {
    if (draft.mode == PlaylistEditorMode::Save) {
        create_playlist(std::move(draft.title), std::move(draft.description), draft.privacy, std::move(draft.cover_path));
    } else {
        edit_playlist(
            std::move(draft.id), std::move(draft.title), std::move(draft.description), draft.privacy, std::move(draft.cover_path)
        );
    }
}

void MusiqApp::create_playlist(
    std::string title, std::string description, MusicPlaylistPrivacy privacy, std::filesystem::path cover_path
) {
    if (!m_session.youtube().user()) {
        m_playlist_editor->fail("Sign in to create playlists.");
        return;
    }

    MusicPlaylistCreateRequest request;
    request.title = title;
    request.description = std::move(description);
    request.privacy = privacy;

    m_tasks.enqueue(
        [session = m_session.youtube(), request = std::move(request)] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).create(request);
        },
        [this, title = std::move(title),
         cover_path = std::move(cover_path)](TaskOutcome<YoutubeResult<MusicPlaylistCreated>> outcome) mutable {
            if (!outcome) {
                m_playlist_editor->fail("Playlist creation task failed.");
                return;
            }

            auto result = std::move(*outcome);
            if (!result) {
                m_playlist_editor->fail(result.error().message);
                return;
            }

            m_catalog.refresh_library();
            m_playlist_editor->finish();

            MusicItem item;
            item.title = std::move(title);
            item.endpoint.browse_id = "VL" + result->id;
            open_playlist(std::move(item));
            upload_playlist_cover(std::move(result->id), std::move(cover_path));
        }
    );
}

void MusiqApp::edit_playlist(
    std::string id, std::string title, std::string description, MusicPlaylistPrivacy privacy, std::filesystem::path cover_path
) {
    MusicPlaylistUpdateRequest request;
    request.playlist_id = id;
    request.title = std::move(title);
    request.description = std::move(description);
    request.privacy = privacy;

    m_tasks.enqueue(
        [session = m_session.youtube(), request = std::move(request)] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).update(request);
        },
        [this, id = std::move(id),
         cover_path = std::move(cover_path)](TaskOutcome<YoutubeResult<MusicPlaylistEditResult>> outcome) mutable {
            if (!outcome) {
                m_playlist_editor->fail("Playlist update task failed.");
                return;
            }

            auto result = std::move(*outcome);
            if (!result) {
                m_playlist_editor->fail(result.error().message);
                return;
            }

            m_catalog.refresh_library();
            m_playlist_editor->finish();
            if (auto* view = dynamic_cast<PlaylistOverlay*>(m_current_page)) {
                view->invalidate();
                view->load(m_session.youtube());
            }

            upload_playlist_cover(std::move(id), std::move(cover_path));
        }
    );
}

void MusiqApp::upload_playlist_cover(std::string id, std::filesystem::path cover_path) {
    if (id.empty() || cover_path.empty()) return;

    MusicPlaylistUpdateRequest request;
    request.playlist_id = id;
    request.thumbnail_path = std::move(cover_path);
    m_tasks.enqueue(
        [session = m_session.youtube(), request = std::move(request)] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).update(request);
        },
        [this, id = std::move(id)](TaskOutcome<YoutubeResult<MusicPlaylistEditResult>> outcome) {
            if (!outcome) {
                LOG_WARN("[playlist] cover upload task failed for {}", id);
                return;
            }

            auto result = std::move(*outcome);
            if (!result) {
                LOG_WARN("[playlist] cover upload failed for {}: {}", id, result.error().message);
                return;
            }

            m_catalog.refresh_library();
            const auto& route = m_history.current();
            const auto browse_id = id.starts_with("VL") ? id : "VL" + id;
            if (route.page == "playlist" && route.query == browse_id) {
                if (auto* view = dynamic_cast<PlaylistOverlay*>(m_current_page)) {
                    view->invalidate();
                    view->load(m_session.youtube());
                }
            }
        }
    );
}

void MusiqApp::update_playback() {
    bool refresh_library = false;
    for (const auto& playlist : m_playlists) {
        if (playlist.second->poll()) {
            m_catalog.remove_playlist(playlist.second->browse_id());
            refresh_library = true;
        }
    }
    if (refresh_library) {
        m_sidebar->set_playlists(m_catalog.library_playlists());
        m_catalog.refresh_library();
    }

    const bool changed = m_playback.update();
    if (!changed) {
        return;
    }

    m_settings_overlay->refresh_playback();
    m_player_bar->refresh();
    m_queue_overlay->refresh();

    if (auto* playlist = dynamic_cast<PlaylistOverlay*>(m_current_page)) {
        playlist->update_playback();
    }
}

void MusiqApp::update_tracks(Node& root) {
    const auto* current = m_playback.queue().current();
    update_track_lists(root, current ? current->media.endpoint.video_id : "");
}

void MusiqApp::update_shortcuts() {
    if (m_loading->visible() || m_channel_picker->native_popup_open() || m_queue_overlay->visible()) return;
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) refresh_page();
}

void MusiqApp::initialize() {
    auto& cache = MusiqCache::instance();

    const auto token = cache.get("auth.session");
    if (!token.empty()) {
        const auto saved = json::parse(token, nullptr, false);
        if (m_session.restore(saved)) {
            update_profile();
            m_session.refresh([this](SessionAction action, YoutubeResult<void> result) {
                session_changed(action, std::move(result));
            });
            load_avatar();
            LOG_INFO("[login] restored cached session");
        } else {
            cache.erase_prefix("auth.");
        }
    }

    if (!m_session.youtube().user()) {
        navigate("home");
    }

    while (!m_surface.is_done()) {
        m_surface.process_events();
        m_tasks.drain();
        update_session();

        if (m_fonts_changed) {
            apply_fonts();
            m_fonts_changed = false;
        }

        m_surface.begin_frame();
        {
            update_shortcuts();
            update_catalog();
            update_playback();

            m_surface.update(ImGui::GetIO().DeltaTime);
            m_surface.draw();

            m_thumbnails.collect();
        }
        m_surface.end_frame();
    }

    // discard ui callbacks before the surface and its widgets are destroyed
    m_tasks.clear();
}
