#include "controller.hpp"
#include "mpv-player.hpp"
#include "../youtube/stream.hpp"
#include "../youtube/client.hpp"
#include "../youtube/queue.hpp"
#include "../youtube/playlists.hpp"
#include "../youtube/catalog.hpp"
#include "../utils/log.hpp"
#include "../utils/media.hpp"

#include <utility>
#include <memory>
#include <algorithm>
#include <cmath>

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

PlaybackController::PlaybackController(MpvPlayer& player)
    : m_player(player), m_media_stream(std::make_shared<YoutubeMediaStream>()) {
    m_queue.on_change([this] {
        m_changed = true;
        if (m_on_queue_change) {
            m_on_queue_change();
        }
    });
    m_media_session.on_play([this] {
        if (m_queue.current() && !resolving() && (paused() || !m_player.state().loaded)) {
            toggle_pause();
        }
    });

    m_media_session.on_pause([this] {
        if (m_player.state().loaded && !m_player.state().paused) {
            m_player.pause(true);
            m_changed = true;
        }
    });

    m_media_session.on_next([this] { m_queue.advance(); });
    m_media_session.on_previous([this] { m_queue.previous(); });
    m_media_session.on_seek([this](double seconds) { seek(seconds); });
    m_media_session.on_shuffle([this](bool enabled) {
        m_queue.set_random(enabled);
        m_media_session.set_shuffle(m_queue.random());
    });
    m_media_session.on_loop([this](const std::string& status) {
        m_queue.set_loop(status == "Track" ? LoopMode::Track : status == "Playlist" ? LoopMode::Playlist : LoopMode::None);
        m_media_session.set_loop(status);
    });
}

void PlaybackController::on_volume(std::function<void(int)> callback) {
    m_media_session.on_volume([this, callback = std::move(callback)](double volume) {
        callback(static_cast<int>(std::lround(std::clamp(volume, 0.0, 1.0) * 100)));
        m_media_session.set_volume(m_player.state().volume / 100.0);
        m_changed = true;
    });
}

void PlaybackController::on_queue_change(std::function<void()> callback) {
    m_on_queue_change = std::move(callback);
}

void PlaybackController::reset_source() {
    m_page_tasks.clear();
    m_page_loading = false;
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
    m_page_tasks.clear();
    m_page_loading = true;

    m_tasks.enqueue(
        m_page_tasks,
        [session = m_session, video_id = std::move(video_id)]() -> YoutubeResult<PlaybackPage> {
            const YoutubeMusicClient client(session);
            auto page = YoutubeMusicQueue(client).next({video_id, {}});
            if (!page) {
                return std::unexpected(std::move(page.error()));
            }

            auto& tracks = page->items;
            // next repeats the seed track already present in the local queue
            if (!tracks.empty() && tracks.front().endpoint.video_id == video_id) {
                tracks.erase(tracks.begin());
            }
            return PlaybackPage{std::move(tracks), std::move(page->continuation)};
        },
        [this](auto result) { append_page(youtube_result(std::move(result))); }
    );
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

    m_page_tasks.clear();
    m_page_loading = true;

    m_tasks.enqueue(
        m_page_tasks, [session = m_session, continuation = *m_continuation] { return read_more(session, continuation); },
        [this](auto result) { append_page(youtube_result(std::move(result))); }
    );
    m_changed = true;
}

void PlaybackController::append_page(YoutubeResult<PlaybackPage> page) {
    m_page_loading = false;
    m_changed = true;

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
    m_media_session.update();

    update_player();
    update_media_session();

    return std::exchange(m_changed, false);
}

void PlaybackController::update_media_session() {
#ifndef _WIN32
    const auto* track = m_queue.current();
    const auto id = track ? track->id : 0;
    const auto& state = m_player.state();

    if (id != m_media_track_id || state.duration != m_media_duration) {
        MediaMetadata metadata;
        if (track) {
            metadata.id = id;
            metadata.title = track->media.title;
            metadata.artist = utils::media::media_artist(track->media);
            metadata.album = track->media.album.text;
            if (!track->media.thumbnails.empty()) metadata.artwork = track->media.thumbnails.back().url;
            metadata.duration = state.duration;
        }
        m_media_session.set_metadata(metadata);
        m_media_track_id = id;
        m_media_duration = state.duration;
    }

    m_media_session.set_playing(track && state.loaded && !paused());
    m_media_session.set_position(state.position);
    m_media_session.set_shuffle(m_queue.random());
    m_media_session.set_loop(
        m_queue.loop() == LoopMode::Track      ? "Track"
        : m_queue.loop() == LoopMode::Playlist ? "Playlist"
                                               : "None"
    );
    m_media_session.set_volume(state.volume / 100.0);
    m_media_session.set_controls(
        track != nullptr, m_queue.can_advance(), m_queue.position() > 0,
        track && track->id == m_track_id && state.loaded && state.duration > 0 && !resolving()
    );
#endif
}

void PlaybackController::seek(double seconds) {
    const auto* track = m_queue.current();
    const auto& state = m_player.state();
    if (!track || track->id != m_track_id || !state.loaded || state.duration <= 0 || resolving()) {
        return;
    }

    seconds = std::clamp(seconds, 0.0, state.duration);
    m_player.seek(seconds);
    m_media_session.seeked(seconds);
    m_changed = true;
}

void PlaybackController::resolve_current() {
    const auto& track = *m_queue.current();

    m_audio_error.clear();
    LOG_INFO("[player] resolving {}", track.media.endpoint.video_id);
    m_stream_tasks.clear();
    m_resolving = true;

    m_tasks.enqueue(
        m_stream_tasks,
        [stream = m_media_stream, session = m_session, video = track.media.endpoint.video_id,
         stop = m_stream_tasks.token()]() -> YoutubeResult<AudioSource> {
            for (unsigned attempt = 0;; ++attempt) {
                if (stop.stop_requested()) return youtube_error(YoutubeErrorKind::Transport, "audio resolution cancelled");
                auto source = stream->resolve(session, video);
                if (source || attempt == 3 || stop.stop_requested()) return source;
                LOG_WARN("[player] audio resolution retry {}/3: {}", attempt + 1, source.error().message);
            }
        },
        [this](auto result) { load_stream(youtube_result(std::move(result))); }
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

    // discard the previous resolver result and recovery state before starting another track
    m_stream_tasks.clear();
    m_resolving = false;

    m_track_id = track ? track->id : 0;
    m_at_end = false;
    m_resume = 0;
    m_audio_error.clear();

    if (track) {
        resolve_current();
    }
}

void PlaybackController::load_stream(YoutubeResult<AudioSource> source) {
    m_resolving = false;
    // the queue may change before the next player update invalidates the old resolver
    const auto* current = m_queue.current();
    if (!current || current->id != m_track_id) {
        return;
    }

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

    const auto& state = m_player.state();

    // a manual retry restores the position after the new stream has loaded
    if (state.loaded && m_resume > 0) {
        seek(std::exchange(m_resume, 0));
    }
}
