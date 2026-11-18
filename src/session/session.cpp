#include "session.hpp"
#include "../database/cache.hpp"

#include <utility>

bool MusiqSession::restore(const json& data) {
    const bool restored = m_youtube.restore(data);
    m_state = restored ? SessionState::Ready : SessionState::SignedOut;
    return restored;
}

bool MusiqSession::running() const {
    return m_state == SessionState::Authenticating || m_state == SessionState::Refreshing ||
           m_state == SessionState::LoadingChannels;
}

void MusiqSession::authenticate(std::string token, std::string identity, Completion completion) {
    run(
        SessionAction::SignIn,
        [token = std::move(token), identity = std::move(identity)](YoutubeMusicSession& session) mutable {
            return session.identities().empty() ? session.authenticate(std::move(token), identity)
                                                : session.select_identity(identity);
        },
        std::move(completion)
    );
}

void MusiqSession::refresh(Completion completion) {
    run(
        SessionAction::Refresh, [](YoutubeMusicSession& session) { return session.select_identity(session.identity()); },
        std::move(completion)
    );
}

void MusiqSession::load_channels(Completion completion) {
    run(SessionAction::Channels, [](YoutubeMusicSession& session) { return session.load_identities(); }, std::move(completion));
}

void MusiqSession::logout() {
    m_scope.clear();
    m_state = SessionState::SignedOut;
    m_youtube = YoutubeMusicSession{};
    MusiqCache::instance().erase_prefix("auth.");
}

void MusiqSession::run(
    SessionAction action, std::function<YoutubeResult<void>(YoutubeMusicSession&)> operation, Completion completion
) {
    m_scope.clear();

    switch (action) {
        case SessionAction::SignIn:
            m_state = SessionState::Authenticating;
            break;
        case SessionAction::Refresh:
            m_state = SessionState::Refreshing;
            break;
        case SessionAction::Channels:
            m_state = SessionState::LoadingChannels;
            break;
    }

    m_tasks.enqueue(
        m_scope,
        [session = m_youtube, operation = std::move(operation)]() mutable -> YoutubeResult<YoutubeMusicSession> {
            auto result = operation(session);
            if (!result) {
                return std::unexpected(std::move(result.error()));
            }
            return session;
        },
        [this, action, completion = std::move(completion)](auto outcome) {
            auto session = youtube_result(std::move(outcome));
            if (session) {
                m_youtube = std::move(*session);
                if (m_youtube.user()) {
                    MusiqCache::instance().set("auth.session", m_youtube.snapshot().dump());
                }
            }

            m_state = m_youtube.user()              ? SessionState::Ready
                      : m_youtube.has_credentials() ? SessionState::SelectingChannel
                                                    : SessionState::SignedOut;
            completion(
                action, session ? YoutubeResult<void>{} : YoutubeResult<void>(std::unexpected(std::move(session.error())))
            );
        }
    );
}
