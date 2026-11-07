#include "playlist.hpp"
#include "../widgets/catalog-card.hpp"
#include "../widgets/text.hpp"
#include "../widgets/media-cover.hpp"
#include "../../playback/controller.hpp"
#include "../../youtube/client.hpp"
#include "../../youtube/playlists.hpp"
#include "../../youtube/catalog.hpp"
#include "../../utils/future.hpp"
#include "../../utils/thread-pool.hpp"
#include "../../utils/log.hpp"

#include <algorithm>
#include <utility>
#include <imgui-ui/layout/virtual-layout.hpp>

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

using namespace ui;

PlaylistOverlay::PlaylistOverlay(MusicItem item, ThumbnailCache& thumbnails, ThreadPool& workers, PlaybackController& playback)
    : Container("playlist", StackDirection::Vertical), m_item(std::move(item)), m_thumbnails(thumbnails), m_workers(workers),
      m_playback(playback) {
    set_type_name("PlaylistOverlay");
    set_size({grow(), grow()});
    set_spacing(16.0f);
    set_visible(false);

    auto& header = add<Container>("details", StackDirection::Horizontal);
    header.set_size({grow(), fit()});
    header.set_spacing(20.0f);
    m_cover = &header.add<MediaCoverWidget>(m_item, m_thumbnails, 180.0f);

    auto& details = header.add<Container>("info", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(8.0f);
    m_title = &details.add<MusiqText>(m_item.title, true);
    m_author = &details.add<MusiqText>(media_artist(m_item));
    m_details = &details.add<MusiqText>("");
    m_description = &details.add<MusiqText>("");

    for (auto* text : {m_title, m_author, m_details, m_description}) {
        text->set_size({grow(), fit()});
        text->set_overflow(TextOverflow::Ellipsis);
    }

    m_play = &details.add<MusiqButtonWidget>("Play");
    m_play->on_click([this] {
        if (m_playback.queue().source() == m_item.endpoint.browse_id) {
            m_playback.toggle_pause();
        } else {
            play(0);
        }
    });

    m_status = &add<MusiqText>("");

    m_tracks = &add<VirtualLayout>("tracks", 64.0f);
    m_tracks->set_size({grow(), grow()});
    m_tracks->set_spacing(6.0f);
    m_tracks->set_overscan(2);
    m_tracks->set_items(0, [this](std::size_t index) -> Node& {
        auto [entry, created] = m_rows.try_emplace(index, nullptr);
        if (!created) return *entry->second;

        auto& card = m_tracks->add<MediaCardWidget>(m_playlist.tracks[index], m_thumbnails);
        card.set_id(std::to_string(index));
        card.on_click([this, index] { play(index); });
        entry->second = &card;
        return card;
    });

    m_more = &add<MusiqButtonWidget>("Load more");
    m_more->on_click([this] { load_more(); });
}

void PlaylistOverlay::invalidate() {
    m_loaded = false;
    m_task = {};
}

void PlaylistOverlay::load(const YoutubeMusicSession& session, bool reload) {
    if (m_loaded && !reload) {
        update_playback();
        return;
    }

    m_loaded = true;
    m_playlist = {};
    m_appending = false;

    m_rows.clear();
    m_tracks->clear();
    m_tracks->set_items(0);
    m_tracks->scroll().seek_to({}, ScrollBehavior::Instant);

    m_session = std::make_shared<const YoutubeMusicSession>(session);
    m_task = m_workers.enqueue([session = m_session, item = m_item] { return read_playlist(*session, item); });

    m_cover->set_item(m_item);
    refresh();
}

void PlaylistOverlay::poll() {
    if (!future_ready(m_task)) return;

    std::string error;
    try {
        auto result = m_task.get();
        if (result) {
            accept_page(std::move(*result));
        } else {
            error = result.error().message;
        }
    } catch (const std::exception& exception) {
        error = exception.what();
    }

    if (!m_appending) {
        MusicItem cover = m_item;
        if (!m_playlist.thumbnails.empty()) cover.thumbnails = m_playlist.thumbnails;
        m_cover->set_item(cover);
    }

    refresh();
    if (!error.empty()) {
        LOG_ERROR("[playlist] page failed: {}", error);
        m_status->set_text(std::move(error));
        m_status->set_visible(true);
    }
}

void PlaylistOverlay::accept_page(MusicPlaylist page) {
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

void PlaylistOverlay::play(std::size_t start) {
    auto& queue = m_playback.queue();
    if (queue.source() == m_item.endpoint.browse_id && start < queue.entries().size()) {
        if (queue.position() == start) {
            m_playback.toggle_pause();
        } else {
            queue.select(queue.entries()[start].id);
        }
        return;
    }
    m_playback.play(PlaybackPage{m_playlist.tracks, m_playlist.continuation}, start, m_item.endpoint.browse_id);
}

void PlaylistOverlay::update_playback() {
    const bool active = m_playback.queue().source() == m_item.endpoint.browse_id;
    m_play->set_text(active ? (m_playback.paused() ? "Resume" : "Pause") : "Play");
}

void PlaylistOverlay::load_more() {
    m_appending = true;
    m_task = m_workers.enqueue([session = m_session, continuation = *m_playlist.continuation]() -> YoutubeResult<MusicPlaylist> {
        const YoutubeMusicClient client(*session);
        auto page = YoutubeMusicPlaylists(client).more(continuation);
        if (!page) return std::unexpected(std::move(page.error()));
        MusicPlaylist result;
        result.tracks = std::move(page->tracks);
        result.continuation = std::move(page->continuation);
        return result;
    });

    refresh();
}

void PlaylistOverlay::refresh() {
    m_title->set_text(m_playlist.title.empty() ? m_item.title : m_playlist.title);
    m_author->set_text(m_playlist.author.text.empty() ? media_artist(m_item) : m_playlist.author.text);
    m_details->set_text(
        m_playlist.year + (m_playlist.year.empty() || m_playlist.duration.empty() ? "" : " · ") + m_playlist.duration
    );
    m_details->set_visible(!m_details->empty());
    m_description->set_text(m_playlist.description);
    m_description->set_visible(!m_description->empty());

    m_play->set_enabled(!m_playlist.tracks.empty());
    update_playback();
    m_tracks->set_items(m_playlist.tracks.size());

    m_status->set_text(m_task.valid() ? "Loading tracks..." : "");
    m_status->set_visible(!m_status->empty());

    m_more->set_visible(m_playlist.continuation.has_value());
    m_more->set_enabled(!m_task.valid());
}

void PlaylistOverlay::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    configure_all_styles([](Style& style) { style.padding({24.0f, 16.0f, 24.0f, 16.0f}); });
}
