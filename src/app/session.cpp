#include "app.hpp"
#include "../database/cache.hpp"
#include "../utils/future.hpp"
#include "../utils/json.hpp"
#include "../utils/log.hpp"
#include "../utils/media.hpp"
#include "../webview/login.hpp"

#include <arbor/resources/texture-registry.hpp>
#include <exception>
#include <utility>

void MusiqApp::sign_in() {
    auto token = open_login(m_surface);
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
        run_session(
            SessionAction::SignIn,
            [token = std::move(token), identity = std::move(identity)](YoutubeMusicSession& session) mutable {
                return session.identities().empty() ? session.authenticate(std::move(token), identity)
                                                    : session.select_identity(identity);
            }
        );

        m_channel_picker->close_popup();
        m_profile->set_busy(true);
        m_loading->show("Signing in...");
        LOG_INFO("[login] starting authentication");
    } catch (const std::exception& error) {
        LOG_ERROR("[login] could not start authentication: {}", error.what());
        m_profile->set_busy(false);
    }
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
    m_avatar = m_workers.enqueue(
        [url = user->avatar_url] { return std::pair{url, utils::media::fetch_thumbnail(url)}; }, TaskPriority::Background
    );
}

void MusiqApp::clear_session() {
    m_session = {};
    m_avatar = {};
    m_youtube_session = YoutubeMusicSession{};
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
    ui::Texture* avatar = nullptr;

    const auto bytes = MusiqCache::instance().get("auth.avatar:" + user.avatar_url);
    if (!bytes.empty()) {
        avatar = m_runtime.textures().add("profile:" + user.avatar_url, std::string_view(bytes));
    }

    profile.set_user(user.name, avatar);
}
