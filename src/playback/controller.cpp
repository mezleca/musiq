#include "controller.hpp"
#include "mpv-player.hpp"
#include "../youtube/stream.hpp"
#include "../youtube/client.hpp"
#include "../youtube/queue.hpp"
#include "../youtube/playlists.hpp"
#include "../youtube/catalog.hpp"
#include "../utils/future.hpp"
#include "../utils/thread-pool.hpp"
#include "../utils/log.hpp"

#include <utility>
#include <memory>

static PlaybackPage catalog_tracks(MusicPage page) {
    PlaybackPage result;
    result.continuation = std::move(page.continuation);
    for (auto& shelf : page.shelves) {
        if (!result.continuation && shelf.continuation) result.continuation = std::move(shelf.continuation);
        for (auto& item : shelf.items) {
            result.tracks.push_back(std::move(item));
        }
    }
    return result;
}

static YoutubeResult<PlaybackPage> read_more(const YoutubeMusicSession& session, const MusicContinuation& continuation) {
    const YoutubeMusicClient client(session);

    if (continuation.route == MusicRoute::Next) {
        auto page = YoutubeMusicQueue(client).more(continuation);
        if (!page) return std::unexpected(std::move(page.error()));
        return PlaybackPage{std::move(page->items), std::move(page->continuation)};
    }

    if (continuation.route == MusicRoute::Playlist) {
        auto page = YoutubeMusicPlaylists(client).more(continuation);
        if (!page) return std::unexpected(std::move(page.error()));
        return PlaybackPage{std::move(page->tracks), std::move(page->continuation)};
    }

    auto page = YoutubeMusicCatalog(client).more(continuation);
    if (!page) {
        return std::unexpected(std::move(page.error()));
    }

    return catalog_tracks(std::move(*page));
}

PlaybackController::PlaybackController(ThreadPool& workers, MpvPlayer& player)
    : m_workers(workers), m_player(player), m_media_stream(std::make_shared<YoutubeMediaStream>()) {}

PlaybackController::~PlaybackController() {
    m_resolve_stop.request_stop();
}

void PlaybackController::reset_source() {
    m_task = {};
    m_continuation.reset();
    m_error.clear();
    m_changed = true;
}

void PlaybackController::set_session(const YoutubeMusicSession& session) {
    m_session = session;
}

void PlaybackController::remove(std::uint64_t id) {
    m_queue.remove(id);
    if (!m_queue.current()) reset_source();
}

void PlaybackController::radio(std::string video_id) {
    m_task = m_workers.enqueue([session = m_session, video_id = std::move(video_id)]() -> YoutubeResult<PlaybackPage> {
        const YoutubeMusicClient client(session);
        auto page = YoutubeMusicQueue(client).next({video_id, {}});
        if (!page) return std::unexpected(std::move(page.error()));

        // next includes its seed track, which is already present in the local queue
        auto& tracks = page->items;
        if (!tracks.empty() && tracks.front().endpoint.video_id == video_id) {
            tracks.erase(tracks.begin());
        }
        return PlaybackPage{std::move(tracks), std::move(page->continuation)};
    });
    m_changed = true;
}

void PlaybackController::play(MusicItem item) {
    reset_source();
    m_queue.replace({item});

    radio(item.endpoint.video_id);
}

void PlaybackController::play(PlaybackPage page, std::size_t start, std::string source) {
    reset_source();
    m_continuation = std::move(page.continuation);
    m_queue.replace(std::move(page.tracks), start, std::move(source));
}

void PlaybackController::load_more() {
    if (loading() || !m_queue.current()) return;

    m_error.clear();
    if (!m_continuation) {
        radio(m_queue.entries().back().media.endpoint.video_id);
        return;
    }

    m_task =
        m_workers.enqueue([session = m_session, continuation = *m_continuation] { return read_more(session, continuation); });
    m_changed = true;
}

void PlaybackController::collect_page() {
    if (!future_ready(m_task)) return;

    m_changed = true;

    auto page = take_result(m_task);
    if (!page) {
        m_error = page.error().message;
        LOG_ERROR("[queue] load failed: {} (status={})", m_error, page.error().status);
        return;
    }

    // recommendations extend the queue without changing the album or playlist that started it
    for (auto& track : page->tracks) {
        if (is_playable(track)) m_queue.append(std::move(track), true);
    }
    m_continuation = std::move(page->continuation);
    LOG_INFO("[queue] loaded page (entries={})", m_queue.entries().size());
}

bool PlaybackController::update() {
    collect_page();
    update_player();
    return std::exchange(m_changed, false);
}

void PlaybackController::resolve_current() {
    const auto& track = *m_queue.current();

    m_audio_error.clear();
    LOG_INFO("[player] resolving {}", track.media.endpoint.video_id);
    m_resolve_stop.request_stop();
    m_resolve_stop = {};

    // keep the resolver and its visitor cache alive until this job finishes
    m_stream_task = m_workers.enqueue(
        [stream = m_media_stream, session = m_session, video = track.media.endpoint.video_id,
         stop = m_resolve_stop.get_token()]() -> YoutubeResult<AudioSource> {
            for (unsigned attempt = 0;; ++attempt) {
                if (stop.stop_requested()) return youtube_error(YoutubeErrorKind::Transport, "audio resolution cancelled");
                auto source = stream->resolve(session, video);
                if (source || attempt == 3 || stop.stop_requested()) return source;
                LOG_WARN("[player] audio resolution retry {}/3: {}", attempt + 1, source.error().message);
            }
        }
    );

    m_changed = true;
}

void PlaybackController::toggle_pause() {
    const auto& state = m_player.state();

    if (!resolving() && (!state.error.empty() || !m_audio_error.empty())) {
        m_resume = state.position;
        m_at_end = false;
        m_player.stop();
        resolve_current();
    } else if (state.loaded || state.buffering) {
        m_player.pause(!state.paused);
    } else if (!resolving()) {
        m_track_id = 0;
    }

    m_changed = true;
}

bool PlaybackController::paused() const {
    return m_player.state().paused || m_at_end || !m_audio_error.empty() || !m_player.state().error.empty();
}

void PlaybackController::switch_track(const QueueEntry* track) {
    m_player.stop();
    m_resolve_stop.request_stop();

    // discard the previous resolver result and recovery state before starting another track
    m_stream_task = {};
    m_track_id = track ? track->id : 0;
    m_at_end = false;
    m_resume = 0;
    m_audio_error.clear();

    if (track) {
        resolve_current();
    }
}

void PlaybackController::collect_stream() {
    if (!future_ready(m_stream_task)) {
        return;
    }

    auto source = take_result(m_stream_task);
    if (source) {
        m_player.load(std::move(*source));
    } else {
        m_audio_error = source.error().message;
        LOG_ERROR("[player] resolve failed: {}", m_audio_error);
    }

    m_changed = true;
}

void PlaybackController::update_player() {
    const auto* current = m_queue.current();
    const bool switched = (current ? current->id : 0) != m_track_id;
    if (switched) {
        switch_track(current);
    }

    m_changed |= m_player.update();

    // ignore end events while replacing a track or resolving its audio
    if (!switched && !resolving() && m_player.state().ended && m_player.state().error.empty()) {
        m_at_end = true;
    }

    // take_repeat consumes the flag so advancing happens once per end event
    if (m_at_end && m_player.state().error.empty()) {
        const bool repeat = m_queue.take_repeat();
        if (repeat || m_queue.advance()) {
            if (repeat) {
                m_player.restart();
            }
            m_at_end = false;
            m_changed = true;
            return;
        }
    }

    collect_stream();
    const auto& state = m_player.state();

    // a manual retry restores the position after the new stream has loaded
    if (state.loaded && m_resume > 0) {
        m_player.seek(std::exchange(m_resume, 0));
    }
}
