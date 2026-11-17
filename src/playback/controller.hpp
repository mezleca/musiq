#pragma once

#include "queue.hpp"
#include "../youtube/session.hpp"
#include "../youtube/schema/continuation.hpp"
#include "../youtube/error.hpp"
#include "audio-source.hpp"
#include "../media/session.hpp"
#include "../utils/task-queue.hpp"

#include <cstdint>
#include <memory>
#include <stop_token>

class MpvPlayer;
class YoutubeMediaStream;

struct PlaybackPage {
    std::vector<MusicItem> tracks;
    std::optional<MusicContinuation> continuation;
};

class PlaybackController {
public:
    explicit PlaybackController(MpvPlayer& player);
    ~PlaybackController();

    PlaybackController(const PlaybackController&) = delete;
    PlaybackController& operator=(const PlaybackController&) = delete;
    PlaybackController(PlaybackController&&) = delete;
    PlaybackController& operator=(PlaybackController&&) = delete;

    void set_session(const YoutubeMusicSession& session);
    void play(MusicItem item);
    void play(PlaybackPage page, std::size_t start = 0, std::string source = {});
    void load_more();
    void remove(std::uint64_t id);
    bool update();
    void toggle_pause();
    void seek(double seconds);
    void on_volume(std::function<void(int)> callback);
    void on_queue_change(std::function<void()> callback);
    bool paused() const;
    bool resolving() const {
        return m_resolving;
    }

    const std::string& playback_error() const {
        return m_audio_error;
    }

    PlaybackQueue& queue() {
        return m_queue;
    }

    bool loading() const {
        return m_page_loading;
    }

    const std::string& error() const {
        return m_error;
    }

private:
    void radio(std::string video_id);
    void reset_source();
    void update_player();
    void switch_track(const QueueEntry* track);
    void resolve_current();
    void finish_page(std::uint64_t generation, TaskOutcome<YoutubeResult<PlaybackPage>> outcome);
    void finish_stream(std::uint64_t generation, TaskOutcome<YoutubeResult<AudioSource>> outcome);
    void update_media_session();

    MpvPlayer& m_player;
    std::function<void()> m_on_queue_change;
    YoutubeMusicSession m_session;
    std::shared_ptr<YoutubeMediaStream> m_media_stream;
    PlaybackQueue m_queue;
    TaskQueue& m_tasks = TaskQueue::instance();
    std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);
    std::optional<MusicContinuation> m_continuation;
    std::string m_error;
    bool m_changed = false;
    std::uint64_t m_page_generation = 0;
    std::uint64_t m_stream_generation = 0;
    bool m_page_loading = false;
    bool m_resolving = false;
    std::stop_source m_resolve_stop;
    std::uint64_t m_track_id = 0;
    std::string m_audio_error;
    bool m_at_end = false;
    double m_resume = 0;
    MediaSession m_media_session{"musiq"};
    std::uint64_t m_media_track_id = 0;
    double m_media_duration = 0;
};
