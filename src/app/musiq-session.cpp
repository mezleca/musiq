#include "musiq-session.hpp"
#include "../database/cache.hpp"

#include <utility>

bool MusiqSession::restore(const json& data) {
    return m_youtube.restore(data);
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
    ++m_generation;
    m_running = false;
    m_youtube = YoutubeMusicSession{};
    MusiqCache::instance().erase_prefix("auth.");
}

void MusiqSession::run(
    SessionAction action, std::function<YoutubeResult<void>(YoutubeMusicSession&)> operation, Completion completion
) {
    const auto generation = ++m_generation;
    const std::weak_ptr<int> lifetime = m_lifetime;
    m_action = action;
    m_running = true;
    m_tasks.enqueue(
        [session = m_youtube, operation = std::move(operation)]() mutable -> YoutubeResult<YoutubeMusicSession> {
            auto result = operation(session);
            if (!result) {
                return std::unexpected(std::move(result.error()));
            }
            return session;
        },
        [this, action, generation, lifetime,
         completion = std::move(completion)](TaskOutcome<YoutubeResult<YoutubeMusicSession>> outcome) mutable {
            if (lifetime.expired() || generation != m_generation) {
                return;
            }
            m_running = false;
            if (!outcome) {
                completion(action, std::unexpected(outcome.error()));
                return;
            }

            auto session = std::move(*outcome);
            if (!session) {
                completion(action, YoutubeResult<void>(std::unexpected(std::move(session.error()))));
                return;
            }

            m_youtube = std::move(*session);
            if (m_youtube.user()) {
                MusiqCache::instance().set("auth.session", m_youtube.snapshot().dump());
            }
            completion(action, YoutubeResult<void>{});
        }
    );
}
