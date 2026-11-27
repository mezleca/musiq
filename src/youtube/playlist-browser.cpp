#include "playlist-browser.hpp"
#include "client.hpp"
#include "playlists.hpp"
#include "catalog.hpp"
#include "../utils/media.hpp"
#include "../utils/log.hpp"

#include <algorithm>
#include <utility>

using namespace utils::media;

YoutubeResult<MusicPlaylist> read_playlist(const YoutubeMusicClient& client, const MusicItem& item) {
    if (item.endpoint.browse_id.starts_with("VL") || item.endpoint.page_type == "MUSIC_PAGE_TYPE_PLAYLIST") {
        return YoutubeMusicPlaylists(client).get(item.endpoint.browse_id);
    }

    // albums have no playlist id, browse the endpoint and flatten its shelves into one track list
    auto page = YoutubeMusicCatalog(client).browse({item.endpoint.browse_id, item.endpoint.params});
    if (!page) return std::unexpected(std::move(page.error()));

    MusicPlaylist result;
    result.title = item.title;
    result.thumbnails = item.thumbnails;
    result.author.text = media_artist(item);
    result.continuation = std::move(page->continuation);

    for (auto& shelf : page->shelves) {
        if (!result.continuation && shelf.continuation) result.continuation = std::move(shelf.continuation);
        for (auto& track : shelf.items) {
            if (track.thumbnails.empty()) track.thumbnails = result.thumbnails;
            if (track.subtitle.empty() && !result.author.text.empty()) track.subtitle = {result.author};
            result.tracks.push_back(std::move(track));
        }
    }

    return result;
}

PlaylistBrowser::PlaylistBrowser(YoutubeMusicConfig config) : m_config(std::move(config)) {}

bool PlaylistBrowser::load(const YoutubeMusicSession& session, const MusicItem& item, PlaylistLoad mode) {
    if (m_state != PlaylistState::Idle && mode == PlaylistLoad::Normal) {
        return false;
    }

    const bool reload = mode == PlaylistLoad::Reload;
    auto previous = reload ? std::move(m_playlist) : MusicPlaylist{};
    auto previous_recommendations = reload ? std::move(m_recommendations) : std::vector<MusicItem>{};
    invalidate();
    if (reload) {
        m_playlist = std::move(previous);
        m_recommendations = std::move(previous_recommendations);
    }

    m_error.clear();

    m_session = session;
    if (mode == PlaylistLoad::NewlyCreated || mode == PlaylistLoad::NewlyCreatedWithCover) {
        m_state = PlaylistState::NewlyCreated;
        m_attempts_left = 3;
        m_wait_for_cover = mode == PlaylistLoad::NewlyCreatedWithCover;
    } else {
        m_state = PlaylistState::Loading;
    }

    queue_load(item);
    return true;
}

void PlaylistBrowser::queue_load(const MusicItem& item) {
    m_tasks.enqueue(
        m_scope,
        [session = m_session, config = m_config, item] { return read_playlist(YoutubeMusicClient(session, config), item); },
        [this](auto result) { accept(youtube_result(std::move(result))); }
    );
}

void PlaylistBrowser::invalidate() {
    m_scope.clear();
    m_recommendations_scope.clear();
    m_recommendations.clear();
    m_recommendations_error.clear();
    m_recommendations_loading = false;
    m_retry.cancel();
    m_playlist = {};
    m_state = PlaylistState::Idle;
    m_change = PlaylistChange::None;
}

bool PlaylistBrowser::append_added_track(MusicItem track) {
    if (m_state != PlaylistState::Ready || m_playlist.continuation) return false;

    if (std::ranges::find(m_playlist.tracks, track.playlist_set_video_id, &MusicItem::playlist_set_video_id) ==
        m_playlist.tracks.end()) {
        m_playlist.tracks.push_back(std::move(track));
        if (m_playlist.track_count) ++*m_playlist.track_count;
    }
    return true;
}

std::optional<std::size_t> PlaylistBrowser::remove_track(std::string_view set_video_id) {
    auto& tracks = m_playlist.tracks;
    const auto track = std::ranges::find(tracks, set_video_id, &MusicItem::playlist_set_video_id);
    if (track == tracks.end()) return std::nullopt;

    const auto index = static_cast<std::size_t>(track - tracks.begin());
    tracks.erase(track);
    if (m_playlist.track_count && *m_playlist.track_count > 0) --*m_playlist.track_count;
    return index;
}

void PlaylistBrowser::set_thumbnails(const std::vector<MusicThumbnail>& thumbnails) {
    m_playlist.thumbnails = thumbnails;
}

bool PlaylistBrowser::load_more() {
    if (m_state != PlaylistState::Ready || !m_playlist.continuation) return false;

    m_state = PlaylistState::Appending;
    m_error.clear();
    m_change = PlaylistChange::None;
    m_scope.clear();

    m_tasks.enqueue(
        m_scope,
        [session = m_session, config = m_config, continuation = *m_playlist.continuation]() -> YoutubeResult<MusicPlaylist> {
            const YoutubeMusicClient client(session, config);
            auto page = YoutubeMusicPlaylists(client).more(continuation);
            if (!page) {
                return std::unexpected(std::move(page.error()));
            }

            MusicPlaylist result;
            result.tracks = std::move(page->tracks);
            result.continuation = std::move(page->continuation);
            return result;
        },
        [this](auto result) { accept(youtube_result(std::move(result))); }
    );

    return true;
}

PlaylistChange PlaylistBrowser::poll(const MusicItem& item) {
    if (m_retry.poll()) queue_load(item);
    return std::exchange(m_change, PlaylistChange::None);
}

void PlaylistBrowser::accept(YoutubeResult<MusicPlaylist> result) {
    const bool created = newly_created();
    const bool missing = !result && result.error().kind == YoutubeErrorKind::Parse &&
                         result.error().message == "missing playlist tracks and header";

    if (created) {
        // the playlist and uploaded cover may become visible at different times
        const bool incomplete = missing || (result && m_wait_for_cover && result->thumbnails.empty());
        if (incomplete && --m_attempts_left != 0) {
            if (result) {
                m_playlist = std::move(*result);
            }
            m_retry.restart();
            return;
        }

        if (!result && !m_playlist.id.empty()) {
            result = std::move(m_playlist);
        }
    }

    if (!result) {
        m_change = missing && !created ? PlaylistChange::Missing : PlaylistChange::Failed;
        m_state = PlaylistState::Failed;
        m_error = missing && created ? "Playlist is not available yet." : result.error().message;
        LOG_ERROR("[playlist] page failed: {}", m_error);
        return;
    }

    auto page = std::move(*result);
    std::erase_if(page.tracks, [](const MusicItem& track) { return !is_playable(track); });

    if (m_state != PlaylistState::Appending) {
        m_change = PlaylistChange::Loaded;
        m_playlist = std::move(page);
        load_recommendations();
    } else {
        m_change = PlaylistChange::Appended;
        for (auto& track : page.tracks) {
            m_playlist.tracks.push_back(std::move(track));
        }
        m_playlist.continuation = std::move(page.continuation);
    }

    m_state = PlaylistState::Ready;
}

void PlaylistBrowser::load_recommendations() {
    if (!m_session.has_credentials() || !m_playlist.owned || !m_playlist.recommendations_continuation) {
        m_recommendations.clear();
        m_recommendations_error.clear();
        return;
    }

    m_recommendations_loading = true;
    m_tasks.enqueue(
        m_recommendations_scope,
        [session = m_session, config = m_config, id = m_playlist.id, continuation = *m_playlist.recommendations_continuation] {
            const YoutubeMusicClient client(session, config);
            return YoutubeMusicPlaylists(client).recommendations(id, continuation);
        },
        [this](auto outcome) {
            auto result = youtube_result(std::move(outcome));
            m_recommendations_loading = false;
            if (result) {
                std::erase_if(result->tracks, [](const MusicItem& track) { return !is_playable(track); });
                m_recommendations = std::move(result->tracks);
            } else {
                m_recommendations_error = result.error().message;
            }
            if (m_change == PlaylistChange::None) m_change = PlaylistChange::Recommendations;
        }
    );
}
