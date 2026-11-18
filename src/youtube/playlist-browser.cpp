#include "playlist-browser.hpp"
#include "client.hpp"
#include "playlists.hpp"
#include "catalog.hpp"
#include "../utils/media.hpp"
#include "../utils/log.hpp"

#include <utility>

using namespace utils::media;

static YoutubeResult<MusicPlaylist> read_playlist(const YoutubeMusicSession& session, const MusicItem& item) {
    const YoutubeMusicClient client(session);

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

bool PlaylistBrowser::load(const YoutubeMusicSession& session, const MusicItem& item, bool reload) {
    if (m_state != PlaylistState::Idle && !reload) return false;

    m_playlist = {};
    m_error.clear();
    m_session = session;
    m_state = PlaylistState::Loading;
    m_change = PlaylistChange::None;
    m_scope.clear();

    m_tasks.enqueue(
        m_scope, [session = m_session, item] { return read_playlist(session, item); },
        [this](auto result) { accept(youtube_result(std::move(result))); }
    );

    return true;
}

void PlaylistBrowser::invalidate() {
    m_scope.clear();
    m_state = PlaylistState::Idle;
    m_change = PlaylistChange::None;
}

bool PlaylistBrowser::load_more() {
    if (m_state != PlaylistState::Ready || !m_playlist.continuation) return false;

    m_state = PlaylistState::Appending;
    m_error.clear();
    m_change = PlaylistChange::None;
    m_scope.clear();

    m_tasks.enqueue(
        m_scope,
        [session = m_session, continuation = *m_playlist.continuation]() -> YoutubeResult<MusicPlaylist> {
            const YoutubeMusicClient client(session);
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

PlaylistChange PlaylistBrowser::poll() {
    return std::exchange(m_change, PlaylistChange::None);
}

void PlaylistBrowser::accept(YoutubeResult<MusicPlaylist> result) {
    if (!result) {
        m_change = PlaylistChange::Failed;
        m_state = PlaylistState::Failed;
        m_error = result.error().message;
        LOG_ERROR("[playlist] page failed: {}", m_error);
        return;
    }

    auto page = std::move(*result);
    std::erase_if(page.tracks, [](const MusicItem& track) { return !is_playable(track); });

    if (m_state == PlaylistState::Loading) {
        m_change = PlaylistChange::Loaded;
        m_playlist = std::move(page);
    } else {
        m_change = PlaylistChange::Appended;
        for (auto& track : page.tracks) {
            m_playlist.tracks.push_back(std::move(track));
        }
        m_playlist.continuation = std::move(page.continuation);
    }

    m_state = PlaylistState::Ready;
}
