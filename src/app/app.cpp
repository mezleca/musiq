#include "app.hpp"
#include "theme.hpp"
#include "widgets/text.hpp"
#include "../utils/media.hpp"
#include "../utils/path.hpp"
#include "../constants.hpp"
#include "../database/cache.hpp"
#include "../database/settings.hpp"
#include "../utils/future.hpp"
#include "../utils/json.hpp"
#include "../utils/log.hpp"
#include "../webview/webview.hpp"

#include <imgui-ui/backends/opengl/texture-loader.hpp>
#include <imgui-ui/backends/sdl/backend.hpp>
#include <imgui-ui/layout/container.hpp>
#include <imgui-ui/resources/texture-registry.hpp>
#include <SDL3/SDL_events.h>
#include <exception>
#include <stdexcept>
#include <memory>
#include <utility>

using namespace ui;
using namespace utils::media;

MusiqApp::MusiqApp(SDL_Window* window, SDL_GLContext context)
    : m_catalog(m_workers), m_thumbnails(m_workers), m_playback(m_workers, m_player),
      m_runtime(
          RuntimeConfig{
              .performance_directory = {},
              .texture_loader = std::make_unique<OpenGLTextureLoader>(),
              .texture_cache = {.cpu_budget_bytes = 128 * 1024 * 1024, .gpu_budget_bytes = 128 * 1024 * 1024},
          }
      ),
      m_surface(
          m_runtime, SurfaceConfig{
                         .backend = std::make_unique<SdlBackend>(window, context),
#ifndef NDEBUG
                         .enable_debugger = true,
#endif
                         .file_dialog_backend = nullptr
                     }
      ) {
    m_surface.set_theme(musiq_theme());
    apply_fonts();

    auto& main = m_surface.root().add<Container>("main", StackDirection::Horizontal);
    main.set_size({grow(), grow()});

    m_sidebar = &main.add<SideBarOverlay>(
        m_thumbnails, [this](std::string_view option) { navigate(option); },
        [this](MusicItem item) { open_playlist(std::move(item)); }
    );

    auto& content = main.add<Container>("content", StackDirection::Vertical);
    content.set_size({grow(), grow()});
    build_navigation(content);
    build_pages(content);
    build_player();

    m_loading = &m_surface.root().add<LoadingOverlay>();
    m_channel_picker = &m_surface.root().add<ChannelPickerOverlay>(*m_profile, [this](std::string identity) {
        authenticate(m_session_token, std::move(identity));
    });
}

void MusiqApp::apply_fonts() {
    auto& settings = MusiqSettings::instance();
    const auto main_font = settings.main_font();
    const auto fallback_font = settings.fallback_font();
    FontRequest fonts;
    if (!main_font.empty()) fonts.main = FontSelection::family(main_font);
    if (!fallback_font.empty()) fonts.fallback = FontSelection::family(fallback_font);

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
    m_back->set_tooltip("Back");
    m_back->on_click([this] {
        if (m_history.back()) show_page();
    });
    m_back->set_enabled(false);

    m_forward = &navigation.add<MusiqIconButtonWidget>("icons/chevron-right.svg", 36.0f);
    m_forward->set_tooltip("Forward");
    m_forward->on_click([this] {
        if (m_history.forward()) show_page();
    });
    m_forward->set_enabled(false);

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
        [this] { clear_browser_cookies(); }, [this] { m_fonts_changed = true; }
    );
}

void MusiqApp::build_player() {
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

std::string MusiqApp::open_sign_in() {
    try {
        std::string token;
        bool reading = false;

        auto window = WebView::create("Sign in");
        window->run(MUSIC_LOGIN_URL, [&] {
            if (reading || !token.empty() || !window->url().starts_with(MUSIC_HOME_URL)) {
                return;
            }
            reading = true;
            window->cookies(MUSIC_HOME_URL, [&](std::string cookies) {
                reading = false;
                if (!YoutubeMusicSession::has_session_cookie(cookies)) {
                    return;
                }
                token = std::move(cookies);
                window->close();
            });
        });
        return token;
    } catch (std::exception& error) {
        LOG_ERROR("[login] failed to open sign-in window: {}", error.what());
    }

    return {};
}

void MusiqApp::sign_in() {
    SDL_Window* window = SDL_GL_GetCurrentWindow();
    SDL_GLContext context = SDL_GL_GetCurrentContext();

    // gtk changes egl state without updating sdl's cached current context
    if (!SDL_GL_MakeCurrent(nullptr, nullptr)) {
        LOG_ERROR("[login] could not release the graphics context: {}", SDL_GetError());
        m_surface.exit();
        return;
    }

    auto token = open_sign_in();

    if (!SDL_GL_MakeCurrent(window, context)) {
        LOG_ERROR("[login] could not restore the graphics context: {}", SDL_GetError());
        m_surface.exit();
        return;
    }

    // discard clicks queued while the modal browser was handling input
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP);
    if (token.empty()) {
        m_profile->set_busy(false);
        return;
    }

    authenticate(std::move(token));
}

void MusiqApp::log_out() {
    clear_session();
    LOG_INFO("[login] logged out");
}

void MusiqApp::clear_browser_cookies() {
    auto* window = SDL_GL_GetCurrentWindow();
    auto context = SDL_GL_GetCurrentContext();
    if (!SDL_GL_MakeCurrent(nullptr, nullptr)) throw std::runtime_error(SDL_GetError());

    std::exception_ptr failure;
    try {
        auto browser = WebView::create("Clear cookies");
        browser->clear_cookies();
    } catch (...) {
        failure = std::current_exception();
    }

    if (!SDL_GL_MakeCurrent(window, context)) {
        m_surface.exit();
        throw std::runtime_error(SDL_GetError());
    }
    if (failure) std::rethrow_exception(failure);
    LOG_INFO("[login] browser cookies cleared");
}

void MusiqApp::run_session(SessionAction action, std::function<YoutubeResult<void>(YoutubeMusicSession&)> task) {
    m_session =
        m_workers.enqueue([session = m_youtube_session, task = std::move(task)]() mutable -> YoutubeResult<YoutubeMusicSession> {
            auto result = task(session);
            if (!result) return std::unexpected(std::move(result.error()));
            return session;
        });
    m_session_action = action;
}

void MusiqApp::authenticate(std::string token, std::string identity) {
    try {
        run_session(SessionAction::SignIn, [token, identity = std::move(identity)](YoutubeMusicSession& session) {
            return session.identities().empty() ? session.authenticate(token, identity) : session.select_identity(identity);
        });

        m_session_token = std::move(token);
        m_channel_picker->close_popup();
        m_profile->set_busy(true);
        m_loading->show("Signing in...");
        LOG_INFO("[login] starting authentication");
    } catch (std::exception& error) {
        LOG_ERROR("[login] could not start authentication: {}", error.what());
        m_profile->set_busy(false);
    }
}

void MusiqApp::refresh_session() {
    run_session(SessionAction::Refresh, [](YoutubeMusicSession& session) { return session.select_identity(session.identity()); });
}

void MusiqApp::choose_channel() {
    m_channel_picker->show(m_youtube_session.identities());
    if (m_session.valid() && m_session_action == SessionAction::Channels) return;
    run_session(SessionAction::Channels, [](YoutubeMusicSession& session) { return session.load_identities(); });
}

void MusiqApp::load_avatar() {
    m_avatar = {};
    const auto& user = m_youtube_session.user();
    if (!user || user->avatar_url.empty() || !MusiqCache::instance().get("auth.avatar:" + user->avatar_url).empty()) return;
    m_avatar =
        m_workers.enqueue([url = user->avatar_url] { return std::pair{url, fetch_thumbnail(url)}; }, TaskPriority::Background);
}

void MusiqApp::clear_session() {
    m_session = {};
    m_avatar = {};
    m_youtube_session = YoutubeMusicSession{};
    m_session_token.clear();
    MusiqCache::instance().erase_prefix("auth.");
    m_loading->set_visible(false);
    m_channel_picker->close_popup();
    m_channel_picker->set_visible(false);
    update_profile();
}

void MusiqApp::cancel_channel_selection() {
    if (m_youtube_session.user()) return;
    clear_session();
    LOG_INFO("[login] channel selection cancelled");
}

void MusiqApp::update_session() {
    if (future_ready(m_avatar)) {
        try {
            auto [url, bytes] = m_avatar.get();
            const auto& user = m_youtube_session.user();
            if (user && user->avatar_url == url && !bytes.empty()) {
                MusiqCache::instance().set("auth.avatar:" + url, bytes);
                refresh_profile();
            }
        } catch (const std::exception& error) {
            LOG_WARN("[login] avatar failed: {}", error.what());
        }
    }

    if (m_channel_picker->take_dismissed() && !m_session.valid()) cancel_channel_selection();
    if (!future_ready(m_session)) return;

    m_loading->set_visible(false);
    m_profile->set_busy(false);

    auto session = take_result(m_session);
    if (!session) {
        const auto& error = session.error();
        LOG_WARN("[login] session request failed: {} (status={})", error.message, error.status);

        // a network outage must not discard a restored session, only an authentication rejection does
        const bool rejected = error.status == 401 || error.status == 403 ||
                              (m_session_action == SessionAction::Refresh && error.kind == YoutubeErrorKind::Authentication);
        if (rejected) {
            clear_session();
            return;
        }

        if (m_session_action == SessionAction::Refresh) {
            return;
        }

        if (m_session_action == SessionAction::Channels) {
            if (m_channel_picker->visible()) m_channel_picker->refresh(m_youtube_session.identities(), true);
            return;
        }

        if (!m_youtube_session.identities().empty()) {
            m_channel_picker->show(m_youtube_session.identities(), true);
        } else if (!m_youtube_session.user()) {
            if (error.kind == YoutubeErrorKind::Authentication) {
                clear_session();
            } else {
                navigate("home");
            }
        }

        return;
    }

    const auto previous_avatar = m_youtube_session.user() ? m_youtube_session.user()->avatar_url : std::string{};
    m_youtube_session = std::move(*session);
    if (m_session_action == SessionAction::Channels) {
        if (m_channel_picker->visible()) m_channel_picker->refresh(m_youtube_session.identities());
        if (m_youtube_session.user()) MusiqCache::instance().set("auth.session", m_youtube_session.snapshot().dump());
        return;
    }

    if (!m_youtube_session.user()) {
        m_channel_picker->show(m_youtube_session.identities());
        return;
    }

    if (m_session_action == SessionAction::Refresh) {
        // metadata refresh must not invalidate the home request already using the restored credentials
        m_playback.set_session(m_youtube_session);
        refresh_profile();
    } else {
        update_profile();
    }

    MusiqCache::instance().set("auth.session", m_youtube_session.snapshot().dump());
    if (!m_avatar.valid() || previous_avatar != m_youtube_session.user()->avatar_url) {
        load_avatar();
    }

    LOG_INFO("[login] session ready");

    if (m_history.current().page == "home" && !m_catalog.home_state().page && !m_catalog.home_state().loading) {
        refresh_page();
    }
}

void MusiqApp::update_profile() {
    m_playback.set_session(m_youtube_session);
    for (const auto& playlist : m_playlists) {
        playlist.second->invalidate();
    }

    m_catalog.set_session(m_youtube_session);
    m_sidebar->set_playlists({});
    show_page();
    refresh_profile();
}

void MusiqApp::refresh_profile() {
    auto& profile = *m_profile;
    m_settings_overlay->set_authenticated(m_youtube_session.user().has_value());
    if (!m_youtube_session.user()) {
        profile.clear_user();
        return;
    }

    const auto& user = *m_youtube_session.user();
    Texture* avatar = nullptr;
    const auto bytes = MusiqCache::instance().get("auth.avatar:" + user.avatar_url);
    if (!bytes.empty()) {
        avatar = m_runtime.textures().add("profile:" + user.avatar_url, std::string_view(bytes));
    }
    profile.set_user(user.name, avatar);
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
        auto* view = m_playlists.at(route.query);
        view->load(m_youtube_session);
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
        m_playlists.at(route.query)->load(m_youtube_session, true);
    }

    show_page();
}

void MusiqApp::update_catalog() {
    const auto changed = m_catalog.update();
    if (changed.library) m_sidebar->set_playlists(m_catalog.library_playlists());

    if (changed.home) {
        m_home->show(m_catalog.home_state(), changed.home_append);
        update_tracks(*m_home);
    }

    if (!changed.home && m_current_page == m_home && m_home->near_bottom() && m_catalog.more_home()) {
        m_home->show(m_catalog.home_state());
    }

    if (!changed.search.empty()) {
        const auto view = m_searches.find(changed.search);
        if (view != m_searches.end()) {
            view->second->show(m_catalog.search_state(changed.search));
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
    auto [entry, created] = m_playlists.try_emplace(id, nullptr);
    if (created) {
        entry->second = &m_pages->add<PlaylistOverlay>(std::move(item), m_thumbnails, m_workers, m_playback);
    }

    m_history.visit({"playlist", id});
    show_page();
}

void MusiqApp::update_playback() {
    for (const auto& playlist : m_playlists) {
        playlist.second->poll();
    }

    const bool changed = m_playback.update();
    const bool queue_changed = m_queue_revision != m_playback.queue().revision();
    if (!changed && !queue_changed) return;

    m_queue_revision = m_playback.queue().revision();
    m_player_bar->refresh();
    m_queue_overlay->refresh();

    for (const auto& playlist : m_playlists) {
        if (playlist.second->visible()) playlist.second->update_playback();
    }

    if (queue_changed) {
        update_tracks(m_surface.root());
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
        if (m_youtube_session.restore(saved)) {
            m_session_token = saved.at("cookies").get<std::string>();
            update_profile();
            refresh_session();
            load_avatar();
            LOG_INFO("[login] restored cached session");
        } else {
            cache.erase_prefix("auth.");
        }
    }

    if (m_session.valid() && m_session_action == SessionAction::SignIn) {
        CatalogPageState waiting;
        waiting.loading = true;
        m_home->show(waiting);
    } else if (!m_youtube_session.user()) {
        navigate("home");
    }

    while (!m_surface.is_done()) {
        m_surface.process_events();
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
}
