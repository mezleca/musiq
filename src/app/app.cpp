#include "app.hpp"
#include "overlays/sidebar.hpp"
#include "overlays/player-bar.hpp"
#include "overlays/loading.hpp"
#include "overlays/settings.hpp"
#include "overlays/channel-picker.hpp"
#include "overlays/home.hpp"
#include "overlays/search.hpp"
#include "widgets/user-profile.hpp"
#include "widgets/search-bar.hpp"
#include "widgets/button.hpp"
#include "widgets/catalog-results.hpp"
#include "resources/avatar.hpp"
#include "../youtube/config.hpp"
#include "../database/settings.hpp"
#include "../utils/json.hpp"
#include "../utils/log.hpp"
#include "../webview/webview.hpp"

#include <imgui-ui/backends/opengl/texture-loader.hpp>
#include <imgui-ui/backends/sdl/backend.hpp>
#include <imgui-ui/layout/container.hpp>
#include <imgui-ui/resources/font-registry.hpp>
#include <imgui-ui/resources/texture-registry.hpp>
#include <imgui-ui/widgets/image.hpp>
#include <SDL3/SDL_events.h>
#include <chrono>
#include <exception>
#include <filesystem>
#include <memory>
#include <utility>

using namespace ui;

class HistoryButtonWidget final : public MusiqButtonWidget {
public:
    explicit HistoryButtonWidget(std::string icon) : MusiqButtonWidget("", {px(36), px(36)}), m_path(std::move(icon)) {
        set_content_alignment(Anchor::Center);
        m_icon = &add<ImageWidget>();
        m_icon->set_size({px(24), px(24)});
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        MusiqButtonWidget::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) {
            style.padding({6.0f}).background_color(theme.transparent).border_thickness(0.0f).border_radius(8.0f);
        });
        style(StyleType::HOVER).background_color(theme.controls.hover_color);
        m_icon->set_texture(surface().runtime().textures().acquire(m_path, std::filesystem::path(m_path)));
        m_icon->configure_all_styles([&theme](Style& style) { style.color(theme.text_color); });
        m_icon->style(StyleType::DISABLED).color(theme.text_secondary_color);
    }

private:
    std::string m_path;
    ImageWidget* m_icon;
};

MusiqApp::MusiqApp(SDL_Window* window, SDL_GLContext context)
    : m_runtime(RuntimeConfig{.performance_directory = {}, .texture_loader = std::make_unique<OpenGLTextureLoader>()}),
      m_surface(
          m_runtime,
          SurfaceConfig{
              .backend = std::make_unique<SdlBackend>(window, context), .enable_debugger = true, .file_dialog_backend = nullptr
          }
      ) {
    auto* font = m_runtime.fonts().add("Manrope Regular", "resources/fonts/manrope-regular.ttf");
    m_surface.set_primary_font(font);
    m_runtime.fonts().add("Manrope Bold", "resources/fonts/manrope-bold.ttf");

    auto& main = m_surface.root().add<Container>("main", StackDirection::Horizontal);
    main.set_size({grow(), grow()});

    m_sidebar = &main.add<SideBarOverlay>([this] { sign_in(); }, [this](std::string_view option) { navigate(option); });

    auto& content = main.add<Container>("content", StackDirection::Vertical);
    content.set_size({grow(), grow()});
    auto& navigation = content.add<Container>("navigation", StackDirection::Horizontal);
    navigation.set_size({grow(), fit()});
    navigation.set_content_alignment(Anchor::CenterLeft);
    navigation.set_spacing(8.0f);
    navigation.configure_all_styles([](Style& style) { style.padding({16.0f}); });

    m_back = &navigation.add<HistoryButtonWidget>("resources/icons/chevron-left.svg");
    m_back->set_tooltip("Back");
    m_back->on_click([this] {
        if (m_history.back()) show_page();
    });
    m_forward = &navigation.add<HistoryButtonWidget>("resources/icons/chevron-right.svg");
    m_forward->set_tooltip("Forward");
    m_forward->on_click([this] {
        if (m_history.forward()) show_page();
    });
    m_back->set_enabled(false);
    m_forward->set_enabled(false);

    m_search_bar = &navigation.add<SearchBarWidget>(
        [this](std::string query) { submit_search(std::move(query)); },
        [this](std::string query) { m_catalog.suggest(std::move(query)); }
    );
    m_pages = &content.add<Container>("pages", StackDirection::Vertical);
    m_pages->set_size({grow(), grow()});
    m_home = &m_pages->add<HomeOverlay>(m_thumbnails);
    m_current_page = m_home;
    m_settings_overlay = &m_pages->add<SettingsOverlay>([this] { log_out(); });

    auto& player = m_surface.root().add<Container>("player", StackDirection::Vertical);
    player.set_size({grow(), fit()});
    player.add<PlayerBarOverlay>();
    m_loading = &m_surface.root().add<LoadingOverlay>();
    m_channel_picker = &m_surface.root().add<ChannelPickerOverlay>(
        [this](std::string identity) { authenticate(m_session_token, std::move(identity)); },
        [this] { cancel_channel_selection(); }
    );
}

std::string MusiqApp::open_sign_in() {
    try {
        std::string token;
        bool reading = false;

        auto window = WebView::create("Sign in");
        window->run(music_config::LOGIN_URL, [&] {
            if (reading || !token.empty() || !window->url().starts_with(music_config::HOME_URL)) {
                return;
            }
            reading = true;
            window->cookies(music_config::HOME_URL, [&](std::string cookies) {
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
        m_sidebar->user_profile().set_busy(false);
        return;
    }

    authenticate(std::move(token));
}

void MusiqApp::log_out() {
    auto& settings = MusiqSettings::instance();
    settings.set_token("");
    m_youtube_session = YoutubeMusicSession{};
    m_session_token.clear();
    update_profile();
    LOG_INFO("[login] logged out");
}

void MusiqApp::authenticate(std::string token, std::string identity) {
    try {
        m_session = std::async(
            std::launch::async,
            [session = m_youtube_session, token, identity = std::move(identity)]() mutable -> std::optional<YoutubeMusicSession> {
                const bool accepted =
                    session.identities().empty() ? session.authenticate(token, identity) : session.select_identity(identity);
                if (!accepted) return std::nullopt;
                return session;
            }
        );

        m_session_token = std::move(token);
        m_channel_picker->set_visible(false);
        m_sidebar->user_profile().set_busy(true);
        m_loading->show();
        m_loading->update("Signing in...");
        LOG_INFO("[login] starting authentication");
    } catch (std::exception& error) {
        LOG_ERROR("[login] could not start authentication: {}", error.what());
        m_sidebar->user_profile().set_busy(false);
    }
}

void MusiqApp::cancel_channel_selection() {
    m_channel_picker->set_visible(false);
    m_youtube_session = YoutubeMusicSession{};
    m_session_token.clear();
    m_sidebar->user_profile().set_busy(false);
    update_profile();
    LOG_INFO("[login] channel selection cancelled");
}

void MusiqApp::update_session() {
    if (!m_session.valid() || m_session.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }

    auto& settings = MusiqSettings::instance();
    m_loading->hide();

    try {
        auto session = m_session.get();
        if (session) {
            m_youtube_session = std::move(*session);
        }
        if (!m_youtube_session.user() && !m_youtube_session.identities().empty()) {
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

    m_session_token.clear();
    m_sidebar->user_profile().set_busy(false);
    if (m_history.current().page == "home" && !m_catalog.home_state().page && !m_catalog.home_state().loading) refresh_home();
}

void MusiqApp::update_profile() {
    refresh_catalog();
    auto& profile = m_sidebar->user_profile();
    m_settings_overlay->set_authenticated(m_youtube_session.user().has_value());
    if (!m_youtube_session.user()) {
        profile.clear_user();
        return;
    }

    const auto& user = *m_youtube_session.user();
    profile.set_user(user.name, load_avatar(m_runtime.textures(), user.avatar_url, user.avatar_data), user.channel_url);
}

void MusiqApp::navigate(std::string_view page) {
    m_history.visit({std::string(page), page == "search" ? m_catalog.search_state().query : std::string{}});
    show_page();
}

void MusiqApp::show_page() {
    const auto& route = m_history.current();
    m_current_page->set_visible(false);
    m_sidebar->select(route.page);
    m_back->set_enabled(m_history.can_back());
    m_forward->set_enabled(m_history.can_forward());
    m_search_bar->set_visible(route.page != "settings");
    m_search_bar->set_query(route.query);
    m_catalog.suggest({});

    if (route.page == "home") {
        if (m_catalog.home()) m_home->results().show(m_catalog.home_state());
        m_current_page = m_home;
    } else if (route.page == "search") {
        const bool loading = m_catalog.search(route.query);
        auto [entry, created] = m_searches.try_emplace(route.query, nullptr);
        if (created) {
            entry->second = &m_pages->add<SearchOverlay>(m_thumbnails);
            entry->second->set_query(route.query);
        }
        if (created || loading) entry->second->results().show(m_catalog.search_state());
        m_current_page = entry->second;
    } else {
        m_current_page = m_settings_overlay;
    }

    m_current_page->set_visible(true);
}

void MusiqApp::submit_search(std::string query) {
    m_history.visit({"search", std::move(query)});
    show_page();
}

void MusiqApp::refresh_home() {
    m_catalog.home(true);
    m_home->results().show(m_catalog.home_state());
}

void MusiqApp::refresh_search() {
    if (m_catalog.search(m_history.current().query, true)) {
        m_searches.at(m_history.current().query)->results().show(m_catalog.search_state());
    }
}

void MusiqApp::refresh_catalog() {
    m_catalog.set_session(m_youtube_session);
    for (const auto& [query, view] : m_searches) {
        view->results().show(m_catalog.search_state(query));
    }

    m_home->results().show(m_catalog.home_state());
    show_page();
}

void MusiqApp::update_catalog() {
    const auto changed = m_catalog.update();
    if (changed.home) m_home->results().show(m_catalog.home_state());
    if (!changed.search.empty()) {
        const auto view = m_searches.find(changed.search);
        if (view != m_searches.end()) view->second->results().show(m_catalog.search_state(changed.search));
    }
    if (changed.suggestions) m_search_bar->set_suggestions(m_catalog.suggestions());
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
        m_home->results().show(waiting);
    } else {
        navigate("home");
    }

    while (!m_surface.is_done()) {
        m_surface.process_events();
        update_session();

        m_surface.begin_frame();
        if (ImGui::IsKeyPressed(ImGuiKey_F5, false) && !m_loading->visible() && !m_channel_picker->visible()) {
            if (m_history.current().page == "home") {
                refresh_home();
            } else if (m_history.current().page == "search") {
                refresh_search();
            }
        }

        update_catalog();
        m_surface.update(ImGui::GetIO().DeltaTime);
        m_surface.draw();
        m_surface.end_frame();
    }
}
