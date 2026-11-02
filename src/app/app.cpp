#include "app.hpp"
#include "overlays/sidebar.hpp"
#include "overlays/player-bar.hpp"
#include "overlays/loading.hpp"
#include "overlays/settings.hpp"
#include "overlays/channel-picker.hpp"
#include "widgets/user-profile.hpp"
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
#include <SDL3/SDL_events.h>
#include <chrono>
#include <exception>
#include <memory>
#include <utility>

using namespace ui;

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

    m_sidebar = &main.add<SideBarOverlay>(
        [this] { sign_in(); }, [this](std::string_view option) { m_settings_overlay->set_visible(option == "settings"); }
    );

    auto& content = main.add<Container>("content", StackDirection::Vertical);
    content.set_size({grow(), grow()});
    m_settings_overlay = &content.add<SettingsOverlay>([this] { log_out(); });

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
}

void MusiqApp::update_profile() {
    auto& profile = m_sidebar->user_profile();
    m_settings_overlay->set_authenticated(m_youtube_session.user().has_value());
    if (!m_youtube_session.user()) {
        profile.clear_user();
        return;
    }

    const auto& user = *m_youtube_session.user();
    profile.set_user(user.name, load_avatar(m_runtime.textures(), user.avatar_url, user.avatar_data), user.channel_url);
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

    while (!m_surface.is_done()) {
        m_surface.process_events();
        update_session();
        m_surface.frame();
    }
}
