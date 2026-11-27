#include "app.hpp"
#include "../database/cache.hpp"
#include "../utils/json.hpp"
#include "../utils/log.hpp"
#include "../utils/media.hpp"
#include "login.hpp"

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

void MusiqApp::authenticate(std::string token, std::string identity) {
    try {
        m_session.authenticate(std::move(token), std::move(identity), [this](SessionAction action, YoutubeResult<void> result) {
            session_changed(action, std::move(result));
        });

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
    m_channel_picker->show(m_session.youtube().identities());
    if (m_session.state() == SessionState::LoadingChannels) return;
    m_session.load_channels([this](SessionAction action, YoutubeResult<void> result) {
        session_changed(action, std::move(result));
    });
}

void MusiqApp::load_avatar() {
    const auto& user = m_session.youtube().user();
    if (!user || user->avatar_url.empty() || !MusiqCache::instance().get("auth.avatar:" + user->avatar_url).empty()) return;
    m_tasks.enqueue(
        [url = user->avatar_url] { return std::pair{url, utils::media::fetch_thumbnail(url)}; },
        [this](TaskOutcome<std::pair<std::string, std::string>> outcome) {
            if (!outcome) {
                try {
                    std::rethrow_exception(outcome.error());
                } catch (const std::exception& error) {
                    LOG_WARN("[login] avatar failed: {}", error.what());
                } catch (...) {
                    LOG_WARN("[login] avatar failed with an unknown exception");
                }
                return;
            }

            auto [url, bytes] = std::move(*outcome);
            const auto& user = m_session.youtube().user();
            if (user && user->avatar_url == url && !bytes.empty()) {
                MusiqCache::instance().set("auth.avatar:" + url, bytes);
                refresh_profile();
            }
        },
        TaskPriority::Background
    );
}

void MusiqApp::clear_session() {
    m_session.logout();
    m_loading->set_visible(false);
    m_channel_picker->close_popup();
    m_channel_picker->set_visible(false);
    update_profile();
}

void MusiqApp::cancel_channel_selection() {
    if (m_session.youtube().user()) return;
    clear_session();
    LOG_INFO("[login] channel selection cancelled");
}

void MusiqApp::update_session() {
    if (m_channel_picker->take_dismissed() && !m_session.running()) cancel_channel_selection();
}

void MusiqApp::session_changed(SessionAction action, YoutubeResult<void> session) {
    m_loading->set_visible(false);
    m_profile->set_busy(false);

    if (!session) {
        const auto& error = session.error();
        LOG_WARN("[login] session request failed: {} (status={})", error.message, error.status);

        // a network outage must not discard a restored session, only an authentication rejection does
        const bool rejected = error.status == 401 || error.status == 403 ||
                              (action == SessionAction::Refresh && error.kind == YoutubeErrorKind::Authentication);
        if (rejected) {
            clear_session();
            return;
        }

        if (action == SessionAction::Refresh) {
            return;
        }

        if (action == SessionAction::Channels) {
            if (m_channel_picker->visible()) m_channel_picker->refresh(m_session.youtube().identities(), true);
            return;
        }

        if (!m_session.youtube().identities().empty()) {
            m_channel_picker->show(m_session.youtube().identities(), true);
        } else if (!m_session.youtube().user()) {
            if (error.kind == YoutubeErrorKind::Authentication) {
                clear_session();
            } else {
                navigate("home");
            }
        }

        return;
    }

    if (action == SessionAction::Channels) {
        if (m_channel_picker->visible()) m_channel_picker->refresh(m_session.youtube().identities());
        return;
    }

    if (!m_session.youtube().user()) {
        m_channel_picker->show(m_session.youtube().identities());
        return;
    }

    if (action == SessionAction::Refresh) {
        // metadata refresh must not invalidate the home request already using the restored credentials
        m_playback.set_session(m_session.youtube());
        refresh_profile();
    } else {
        update_profile();
    }

    load_avatar();

    LOG_INFO("[login] session ready");

    if (m_history.current().page == "home" && !m_catalog.home_state().page && !m_catalog.home_state().loading) {
        show_page();
    }
}

void MusiqApp::update_profile() {
    m_playback.set_session(m_session.youtube());
    for (const auto& playlist : m_playlists) {
        playlist.second->invalidate();
    }

    m_catalog.set_session(m_session.youtube());
    m_sidebar->set_playlists({});
    show_page();
    refresh_profile();
}

void MusiqApp::refresh_profile() {
    auto& profile = *m_profile;
    m_settings_overlay->set_authenticated(m_session.youtube().user().has_value());
    if (!m_session.youtube().user()) {
        profile.clear_user();
        return;
    }

    const auto& user = *m_session.youtube().user();
    ui::Texture* avatar = nullptr;

    const auto bytes = MusiqCache::instance().get("auth.avatar:" + user.avatar_url);
    if (!bytes.empty()) {
        avatar = m_runtime.textures().add("profile:" + user.avatar_url, std::string_view(bytes));
    }

    profile.set_user(user.name, avatar);
}
