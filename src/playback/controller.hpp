#pragma once

#include "queue.hpp"
#include "../youtube/session.hpp"
#include "../youtube/schema/continuation.hpp"
#include "../youtube/error.hpp"
#include "audio-source.hpp"

#include <future>
#include <memory>

class ThreadPool;
class MpvPlayer;
class YoutubeMediaStream;

struct PlaybackPage {
    std::vector<MusicItem> tracks;
    std::optional<MusicContinuation> continuation;
};

class PlaybackController {
public:
    PlaybackController(ThreadPool& workers, MpvPlayer& player);
    void set_session(const YoutubeMusicSession& session);
    void play(MusicItem item);
    void play(PlaybackPage page, std::size_t start = 0, std::string source = {});
    void load_more();
    void remove(std::uint64_t id);
    bool update();
    void toggle_pause();
    bool paused() const;
    bool resolving() const {
        return m_stream_task.valid();
    }
    const std::string& playback_error() const {
        return m_audio_error;
    }

    PlaybackQueue& queue() {
        return m_queue;
    }

    bool loading() const {
        return m_task.valid();
    }

    const std::string& error() const {
        return m_error;
    }

private:
    void collect_page();
    void radio(std::string video_id);
    void reset_source();
    void update_player();
    void switch_track(const QueueEntry* track);
    void resolve_current();
    void collect_stream();

    ThreadPool& m_workers;
    MpvPlayer& m_player;
    std::shared_ptr<const YoutubeMusicSession> m_session;
    std::shared_ptr<YoutubeMediaStream> m_media_stream;
    PlaybackQueue m_queue;
    std::future<YoutubeResult<PlaybackPage>> m_task;
    std::optional<MusicContinuation> m_continuation;
    std::string m_error;
    bool m_changed = false;
    std::future<YoutubeResult<AudioSource>> m_stream_task;
    std::uint64_t m_track_id = 0;
    std::string m_audio_error;
    bool m_at_end = false;
    bool m_retried = false;
    double m_resume = 0;
};
