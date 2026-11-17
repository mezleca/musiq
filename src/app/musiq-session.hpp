#pragma once

#include "../utils/task-queue.hpp"
#include "../youtube/session.hpp"

#include <functional>
#include <cstdint>
#include <string>

enum class SessionAction {
    SignIn,
    Refresh,
    Channels
};

class MusiqSession {
public:
    using Completion = std::function<void(SessionAction, TaskOutcome<YoutubeResult<void>>)>;

    void run(SessionAction action, std::function<YoutubeResult<void>(YoutubeMusicSession&)> operation, Completion completion);

    const YoutubeMusicSession& youtube() const {
        return m_youtube;
    }

    bool restore(const json& data);
    void authenticate(std::string token, std::string identity, Completion completion);
    void refresh(Completion completion);
    void load_channels(Completion completion);
    void logout();

    bool running() const {
        return m_running;
    }

    SessionAction action() const {
        return m_action;
    }

private:
    YoutubeMusicSession m_youtube;
    TaskQueue& m_tasks = TaskQueue::instance();
    std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);
    SessionAction m_action = SessionAction::SignIn;
    std::uint64_t m_generation = 0;
    bool m_running = false;
};
