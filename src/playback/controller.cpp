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
    if (!page) return std::unexpected(std::move(page.error()));
    return catalog_tracks(std::move(*page));
}

PlaybackController::PlaybackController(ThreadPool& workers, MpvPlayer& player)
    : m_workers(workers), m_player(player), m_session(std::make_shared<const YoutubeMusicSession>()) {}

void PlaybackController::reset_source() {
    // the request may still finish, but its result must not enter the replacement queue
    m_task = {};
    m_continuation.reset();
    m_error.clear();
    m_radio = true;
    m_changed = true;
}

void PlaybackController::set_session(const YoutubeMusicSession& session) {
    m_session = std::make_shared<const YoutubeMusicSession>(session);
}

void PlaybackController::remove(std::uint64_t id) {
    m_queue.remove(id);
    if (!m_queue.current()) reset_source();
}

void PlaybackController::radio(std::string video_id) {
    m_radio = true;
    m_task = m_workers.enqueue([session = m_session, video_id = std::move(video_id)]() -> YoutubeResult<PlaybackPage> {
        const YoutubeMusicClient client(*session);
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

    if (m_autoplay) radio(item.endpoint.video_id);
}

void PlaybackController::play(PlaybackPage page, std::size_t start, std::string source) {
    reset_source();
    m_radio = false;

    m_continuation = std::move(page.continuation);
    m_queue.replace(std::move(page.tracks), start, std::move(source));
}

void PlaybackController::set_autoplay(bool enabled) {
    m_autoplay = enabled;
    m_changed = true;
}

void PlaybackController::load_more() {
    if (loading() || !m_continuation) return;

    m_error.clear();
    m_task =
        m_workers.enqueue([session = m_session, continuation = *m_continuation] { return read_more(*session, continuation); });
    m_changed = true;
}

void PlaybackController::collect_page() {
    if (!future_ready(m_task)) return;

    m_changed = true;

    try {
        auto page = m_task.get();
        if (!page) {
            m_error = page.error().message;
            LOG_ERROR("[queue] load failed: {} (status={})", m_error, page.error().status);
            return;
        }

        for (auto& track : page->tracks) {
            if (is_playable(track)) m_queue.append(std::move(track), !m_radio);
        }
        m_continuation = std::move(page->continuation);
        LOG_INFO("[queue] loaded page (entries={})", m_queue.entries().size());
    } catch (const std::exception& error) {
        m_error = error.what();
        LOG_ERROR("[queue] load failed: {}", m_error);
    }
}

bool PlaybackController::update() {
    collect_page();

    update_player();

    if (!loading() && m_error.empty() && m_queue.current() && m_queue.remaining() <= 3) {
        // prefetch the next page, or start radio when the source has no continuation
        if (m_continuation && (m_continuation->route != MusicRoute::Next || m_autoplay)) {
            load_more();
        } else if (!m_continuation && m_autoplay && !m_radio) {
            radio(m_queue.entries().back().media.endpoint.video_id);
        }
    }
    return std::exchange(m_changed, false);
}

void PlaybackController::resolve_current() {
    const auto& track = *m_queue.current();
    m_audio_error.clear();
    LOG_INFO("[player] resolving {}", track.media.endpoint.video_id);

    m_stream_task = m_workers.enqueue([session = m_session, video = track.media.endpoint.video_id] {
        return resolve_audio(*session, video);
    });
    m_changed = true;
}

void PlaybackController::toggle_pause() {
    const auto& state = m_player.state();
    if (state.loaded || state.buffering) {
        m_player.pause(!state.paused);
    } else if (!resolving()) {
        m_track_id = 0;
    }
    m_changed = true;
}

bool PlaybackController::paused() const {
    return m_player.state().paused || m_at_end || !m_audio_error.empty();
}

void PlaybackController::update_player() {
    const auto* current = m_queue.current();
    const auto id = current ? current->id : 0;
    const bool switched = id != m_track_id;
    if (switched) {
        m_player.stop();
        m_stream_task = {};
        m_track_id = id;
        m_at_end = false;
        m_retried = false;
        m_resume = 0;
        m_audio_error.clear();
        if (current) resolve_current();
    }

    m_changed |= m_player.update();
    if (!switched && !resolving() && m_player.state().ended) {
        m_at_end = true;
    }

    if (m_at_end && m_queue.advance()) {
        m_at_end = false;
        m_changed = true;
        return;
    }

    if (future_ready(m_stream_task)) {
        try {
            auto source = m_stream_task.get();
            if (source) {
                m_player.load(std::move(*source));
            } else {
                m_audio_error = source.error().message;
            }
        } catch (const std::exception& error) {
            m_audio_error = error.what();
        }
        if (!m_audio_error.empty()) LOG_ERROR("[player] resolve failed: {}", m_audio_error);
        m_changed = true;
    }

    const auto& state = m_player.state();
    if (state.loaded && m_resume > 0) {
        m_player.seek(std::exchange(m_resume, 0));
    }

    if (current && !resolving() && !state.error.empty() && !m_retried) {
        // a failed stream may be transient, resolve once more and resume from the last position
        m_retried = true;
        m_resume = state.position;
        m_player.stop();
        LOG_WARN("[player] stream failed, resolving once more");
        resolve_current();
    }
}
