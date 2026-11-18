#pragma once

#include "session.hpp"
#include "schema/playlist.hpp"
#include "error.hpp"
#include "../utils/task-queue.hpp"

#include <cstdint>
#include <memory>
#include <string>

enum class PlaylistState {
    Idle,
    Loading,
    Appending,
    Ready,
    Failed
};

enum class PlaylistChange {
    None,
    Loaded,
    Appended,
    Failed
};

class PlaylistBrowser {
public:
    PlaylistBrowser() = default;

    bool load(const YoutubeMusicSession& session, const MusicItem& item, bool reload = false);
    void invalidate();
    bool load_more();
    PlaylistChange poll();

    const MusicPlaylist& playlist() const {
        return m_playlist;
    }

    const std::string& error() const {
        return m_error;
    }

    bool loading() const {
        return m_state == PlaylistState::Loading || m_state == PlaylistState::Appending;
    }

    bool appending() const {
        return m_state == PlaylistState::Appending;
    }

private:
    void accept(YoutubeResult<MusicPlaylist> result);

    TaskQueue& m_tasks = TaskQueue::instance();
    YoutubeMusicSession m_session;
    MusicPlaylist m_playlist;
    std::string m_error;
    TaskScope m_scope;
    PlaylistState m_state = PlaylistState::Idle;
    PlaylistChange m_change = PlaylistChange::None;
};
