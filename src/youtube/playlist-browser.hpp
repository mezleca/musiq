#pragma once

#include "session.hpp"
#include "schema/playlist.hpp"
#include "error.hpp"
#include "../utils/task-queue.hpp"

#include <cstdint>
#include <memory>
#include <string>

class PlaylistBrowser {
public:
    PlaylistBrowser();

    bool load(const YoutubeMusicSession& session, const MusicItem& item, bool reload = false);
    void invalidate();
    void load_more();
    bool poll();

    const MusicPlaylist& playlist() const {
        return m_playlist;
    }

    const std::string& error() const {
        return m_error;
    }

    bool loading() const {
        return m_loading;
    }

    bool appending() const {
        return m_appending;
    }

private:
    void accept_page(MusicPlaylist page);
    void accept_task(std::uint64_t generation, TaskOutcome<YoutubeResult<MusicPlaylist>> outcome);

    TaskQueue& m_tasks = TaskQueue::instance();
    YoutubeMusicSession m_session;
    MusicPlaylist m_playlist;
    std::string m_error;
    std::uint64_t m_generation = 0;
    std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);
    bool m_appending = false;
    bool m_loaded = false;
    bool m_loading = false;
    bool m_changed = false;
};
