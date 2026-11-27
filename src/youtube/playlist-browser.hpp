#pragma once

#include "session.hpp"
#include "schema/playlist.hpp"
#include "error.hpp"
#include "../utils/debounce.hpp"
#include "../utils/task-queue.hpp"

#include <cstdint>
#include <memory>
#include <string>

class YoutubeMusicClient;

YoutubeResult<MusicPlaylist> read_playlist(const YoutubeMusicClient& client, const MusicItem& item);

enum class PlaylistState {
    Idle,
    Loading,
    NewlyCreated,
    Appending,
    Ready,
    Failed
};

enum class PlaylistChange {
    None,
    Loaded,
    Appended,
    Failed,
    Missing,
    Recommendations
};

enum class PlaylistLoad {
    Normal,
    Reload,
    NewlyCreated,
    NewlyCreatedWithCover
};

class PlaylistBrowser {
public:
    explicit PlaylistBrowser(YoutubeMusicConfig config = {});

    bool load(const YoutubeMusicSession& session, const MusicItem& item, PlaylistLoad mode = PlaylistLoad::Normal);
    void invalidate();
    bool load_more();
    bool append_added_track(MusicItem track);
    std::optional<std::size_t> remove_track(std::string_view set_video_id);
    void set_thumbnails(const std::vector<MusicThumbnail>& thumbnails);
    PlaylistChange poll(const MusicItem& item);

    const MusicPlaylist& playlist() const {
        return m_playlist;
    }

    const std::vector<MusicItem>& recommendations() const {
        return m_recommendations;
    }

    bool recommendations_loading() const {
        return m_recommendations_loading;
    }

    const std::string& recommendations_error() const {
        return m_recommendations_error;
    }

    const std::string& error() const {
        return m_error;
    }

    bool loading() const {
        return m_state == PlaylistState::Loading || newly_created() || appending();
    }

    bool newly_created() const {
        return m_state == PlaylistState::NewlyCreated;
    }

    bool appending() const {
        return m_state == PlaylistState::Appending;
    }

private:
    void queue_load(const MusicItem& item);
    void accept(YoutubeResult<MusicPlaylist> result);
    void load_recommendations();

    TaskQueue& m_tasks = TaskQueue::instance();
    YoutubeMusicConfig m_config;
    YoutubeMusicSession m_session;
    MusicPlaylist m_playlist;
    std::vector<MusicItem> m_recommendations;
    std::string m_recommendations_error;
    std::string m_error;
    TaskScope m_scope;
    TaskScope m_recommendations_scope;
    PlaylistState m_state = PlaylistState::Idle;
    PlaylistChange m_change = PlaylistChange::None;
    Debounce m_retry{std::chrono::seconds(1)};
    unsigned m_attempts_left = 0;
    bool m_wait_for_cover = false;
    bool m_recommendations_loading = false;
};
