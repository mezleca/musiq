#include "playlist-browser.hpp"
#include "client.hpp"
#include "playlists.hpp"
#include "catalog.hpp"
#include "../utils/media.hpp"
#include "../utils/future.hpp"
#include "../utils/thread-pool.hpp"
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

PlaylistBrowser::PlaylistBrowser(ThreadPool& workers) : m_workers(workers) {}

bool PlaylistBrowser::load(const YoutubeMusicSession& session, const MusicItem& item, bool reload) {
    if (m_loaded && !reload) return false;

    m_loaded = true;
    m_playlist = {};
    m_appending = false;
    m_error.clear();
    m_session = session;
    m_task = m_workers.enqueue([session = m_session, item] { return read_playlist(session, item); });
    return true;
}

void PlaylistBrowser::invalidate() {
    m_loaded = false;
    m_task = {};
}

void PlaylistBrowser::load_more() {
    m_appending = true;
    m_error.clear();
    m_task = m_workers.enqueue([session = m_session, continuation = *m_playlist.continuation]() -> YoutubeResult<MusicPlaylist> {
        const YoutubeMusicClient client(session);
        auto page = YoutubeMusicPlaylists(client).more(continuation);
        if (!page) return std::unexpected(std::move(page.error()));
        MusicPlaylist result;
        result.tracks = std::move(page->tracks);
        result.continuation = std::move(page->continuation);
        return result;
    });
}

bool PlaylistBrowser::poll() {
    if (!future_ready(m_task)) return false;

    auto result = take_result(m_task);
    if (result) {
        accept_page(std::move(*result));
    } else {
        m_error = result.error().message;
        LOG_ERROR("[playlist] page failed: {}", m_error);
    }

    return true;
}

void PlaylistBrowser::accept_page(MusicPlaylist page) {
    std::erase_if(page.tracks, [](const MusicItem& track) { return !is_playable(track); });
    if (!m_appending) {
        m_playlist = std::move(page);
        return;
    }

    for (auto& track : page.tracks) {
        m_playlist.tracks.push_back(std::move(track));
    }

    m_playlist.continuation = std::move(page.continuation);
}
