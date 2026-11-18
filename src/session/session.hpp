#pragma once

#include "../utils/task-queue.hpp"
#include "../youtube/session.hpp"

#include <functional>
#include <string>

enum class SessionAction {
    SignIn,
    Refresh,
    Channels
};

enum class SessionState {
    SignedOut,
    Authenticating,
    Refreshing,
    LoadingChannels,
    SelectingChannel,
    Ready
};

class MusiqSession {
public:
    using Completion = std::function<void(SessionAction, YoutubeResult<void>)>;

    const YoutubeMusicSession& youtube() const {
        return m_youtube;
    }

    bool restore(const json& data);
    void authenticate(std::string token, std::string identity, Completion completion);
    void refresh(Completion completion);
    void load_channels(Completion completion);
    void logout();

    bool running() const;
    SessionState state() const {
        return m_state;
    }

private:
    void run(SessionAction action, std::function<YoutubeResult<void>(YoutubeMusicSession&)> operation, Completion completion);

    YoutubeMusicSession m_youtube;
    TaskQueue& m_tasks = TaskQueue::instance();
    TaskScope m_scope;
    SessionState m_state = SessionState::SignedOut;
};
