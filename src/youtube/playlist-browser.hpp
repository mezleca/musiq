#pragma once

#include "schema/playlist.hpp"
#include "error.hpp"

#include <future>
#include <memory>
#include <string>

class ThreadPool;
class YoutubeMusicSession;

class PlaylistBrowser {
public:
    explicit PlaylistBrowser(ThreadPool& workers);

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
        return m_task.valid();
    }

    bool appending() const {
        return m_appending;
    }

private:
    void accept_page(MusicPlaylist page);

    ThreadPool& m_workers;
    std::shared_ptr<const YoutubeMusicSession> m_session;
    MusicPlaylist m_playlist;
    std::future<YoutubeResult<MusicPlaylist>> m_task;
    std::string m_error;
    bool m_appending = false;
    bool m_loaded = false;
};
