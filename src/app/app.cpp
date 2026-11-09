#include "app.hpp"
#include "theme.hpp"
#include "../utils/media.hpp"
#include "../utils/path.hpp"
#include "../constants.hpp"
#include "../database/settings.hpp"
#include "../utils/future.hpp"
#include "../utils/json.hpp"
#include "../utils/log.hpp"
#include "../webview/webview.hpp"

#include <imgui-ui/backends/opengl/texture-loader.hpp>
#include <imgui-ui/backends/sdl/backend.hpp>
#include <imgui-ui/layout/container.hpp>
#include <imgui-ui/resources/font-registry.hpp>
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
    auto* font = m_runtime.fonts().add("Manrope Regular", utils::path::from_resources("fonts/manrope-regular.ttf"));
    m_surface.set_primary_font(font);
    m_runtime.fonts().add("Manrope Bold", utils::path::from_resources("fonts/manrope-bold.ttf"));

    auto& main = m_surface.root().add<Container>("main", StackDirection::Horizontal);
    main.set_size({grow(), grow()});

    m_sidebar = &main.add<SideBarOverlay>([this](std::string_view option) { navigate(option); });

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

    navigation.add<Container>("profile-spacer").set_size({grow(), px(1)});
    m_profile = &navigation.add<UserProfileWidget>(
        [this] { sign_in(); }, [this] { m_channel_picker->show(m_youtube_session.identities()); }
    );
}

void MusiqApp::build_pages(Container& content) {
    m_pages = &content.add<Container>("pages", StackDirection::Vertical);
    m_pages->set_size({grow(), grow()});

    m_home = &m_pages->add<CatalogResultsWidget>("home", "Home", m_thumbnails);
    m_home->set_actions(catalog_actions());
    m_current_page = m_home;

    m_settings_overlay = &m_pages->add<SettingsOverlay>(
        [this] { log_out(); },
        [this](int volume) {
            m_player_bar->set_volume(volume);
            m_player.set_volume(volume);
        },
        [this] { clear_browser_cookies(); }
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
    auto& settings = MusiqSettings::instance();
    settings.set_token("");
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

void MusiqApp::authenticate(std::string token, std::string identity) {
    try {
        m_session = m_workers.enqueue(
            [session = m_youtube_session, token, identity = std::move(identity)]() mutable -> std::optional<YoutubeMusicSession> {
                const bool accepted =
                    session.identities().empty() ? session.authenticate(token, identity) : session.select_identity(identity);
                if (!accepted) return std::nullopt;
                return session;
            }
        );

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

void MusiqApp::clear_session() {
    m_youtube_session = YoutubeMusicSession{};
    m_session_token.clear();
    update_profile();
}

void MusiqApp::cancel_channel_selection() {
    if (m_youtube_session.user()) return;
    clear_session();
    LOG_INFO("[login] channel selection cancelled");
}

void MusiqApp::update_session() {
    if (m_channel_picker->take_dismissed() && !m_session.valid()) cancel_channel_selection();
    if (!future_ready(m_session)) return;

    auto& settings = MusiqSettings::instance();
    m_loading->set_visible(false);

    try {
        auto session = m_session.get();
        if (session) {
            m_youtube_session = std::move(*session);
        }

        if ((!session || !m_youtube_session.user()) && !m_youtube_session.identities().empty()) {
            // keep m_session_token so the picker can resubmit it through authenticate()
            m_channel_picker->show(m_youtube_session.identities(), !session);
            return;
        }

        if (session) {
            update_profile();
            LOG_INFO("[login] successfully logged in");
            settings.set_token(json{{"cookies", m_session_token}, {"identity", m_youtube_session.identity()}}.dump());
        }
    } catch (std::exception& error) {
        LOG_ERROR("[login] could not complete sign-in: {}", error.what());
    }

    m_profile->set_busy(false);
    if (m_history.current().page == "home" && !m_catalog.home_state().page && !m_catalog.home_state().loading) refresh_page();
}

void MusiqApp::update_profile() {
    m_playback.set_session(m_youtube_session);
    for (const auto& playlist : m_playlists) {
        playlist.second->invalidate();
    }

    m_catalog.set_session(m_youtube_session);
    show_page();

    auto& profile = *m_profile;
    m_settings_overlay->set_authenticated(m_youtube_session.user().has_value());
    if (!m_youtube_session.user()) {
        profile.clear_user();
        return;
    }

    const auto& user = *m_youtube_session.user();
    Texture* avatar = nullptr;
    if (!user.avatar_data.empty()) {
        avatar = m_runtime.textures().add("profile:" + user.avatar_url, std::string_view(user.avatar_data));
    }
    profile.set_user(user.name, avatar);
}

void MusiqApp::navigate(std::string_view page) {
    m_history.visit({std::string(page), page == "search" ? m_catalog.search_state().query : std::string{}});
    show_page();
}

void MusiqApp::show_page() {
    const auto& route = m_history.current();
    m_sidebar->select(route.page);
    m_back->set_enabled(m_history.can_back());
    m_forward->set_enabled(m_history.can_forward());
    m_search_bar->set_visible(route.page != "settings");
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

    if (changed.home) m_home->show(m_catalog.home_state());
    if (!changed.search.empty()) {
        const auto view = m_searches.find(changed.search);
        if (view != m_searches.end()) view->second->show(m_catalog.search_state(changed.search));
    }
    if (changed.suggestions) m_search_bar->set_suggestions(m_catalog.suggestions());
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
    if (!changed && m_queue_revision == m_playback.queue().revision()) return;

    m_queue_revision = m_playback.queue().revision();
    m_player_bar->refresh();
    m_queue_overlay->refresh();
    for (const auto& playlist : m_playlists) {
        if (playlist.second->visible()) playlist.second->update_playback();
    }
}

void MusiqApp::update_shortcuts() {
    if (m_loading->visible() || m_channel_picker->native_popup_open() || m_queue_overlay->visible()) return;
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) refresh_page();
}

void MusiqApp::initialize() {
    auto& settings = MusiqSettings::instance();
    auto token = settings.token();

    if (!token.empty()) {
        const auto saved = json::parse(token, nullptr, false);
        if (saved.is_object() && saved.contains("cookies")) {
            authenticate(saved.at("cookies").get<std::string>(), saved.value("identity", std::string{}));
        } else {
            authenticate(std::move(token));
        }
    }

    if (m_session.valid()) {
        CatalogPageState waiting;
        waiting.loading = true;
        m_home->show(waiting);
    } else {
        navigate("home");
    }

    while (!m_surface.is_done()) {
        m_surface.process_events();
        update_session();

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
