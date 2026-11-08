#include "playlist.hpp"
#include "../../utils/media.hpp"
#include "../widgets/catalog-card.hpp"
#include "../widgets/text.hpp"
#include "../widgets/media-cover.hpp"
#include "../../playback/controller.hpp"

#include <utility>
#include <imgui-ui/layout/virtual-layout.hpp>

using namespace ui;
using namespace media;

PlaylistOverlay::PlaylistOverlay(MusicItem item, ThumbnailCache& thumbnails, ThreadPool& workers, PlaybackController& playback)
    : Container("playlist", StackDirection::Vertical), m_item(std::move(item)), m_thumbnails(thumbnails), m_playback(playback),
      m_browser(workers) {
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

        auto& card = m_tracks->add<MediaCardWidget>(m_browser.playlist().tracks[index], m_thumbnails);
        card.set_id(std::to_string(index));
        card.on_click([this, index] { play(index); });
        entry->second = &card;
        return card;
    });

    m_more = &add<MusiqButtonWidget>("Load more");
    m_more->on_click([this] { load_more(); });
}

void PlaylistOverlay::invalidate() {
    m_browser.invalidate();
}

void PlaylistOverlay::load(const YoutubeMusicSession& session, bool reload) {
    if (!m_browser.load(session, m_item, reload)) {
        update_playback();
        return;
    }

    m_rows.clear();
    m_tracks->clear();
    m_tracks->set_items(0);
    m_tracks->scroll().seek_to({}, ScrollBehavior::Instant);

    m_cover->set_item(m_item);
    refresh();
}

void PlaylistOverlay::poll() {
    if (!m_browser.poll()) return;

    const auto& playlist = m_browser.playlist();
    if (!m_browser.appending()) {
        MusicItem cover = m_item;
        if (!playlist.thumbnails.empty()) cover.thumbnails = playlist.thumbnails;
        m_cover->set_item(cover);
    }

    refresh();
    if (!m_browser.error().empty()) {
        m_status->set_text(m_browser.error());
        m_status->set_visible(true);
    }
}

void PlaylistOverlay::play(std::size_t start) {
    auto& queue = m_playback.queue();
    if (queue.source() != m_item.endpoint.browse_id || start >= queue.entries().size()) {
        const auto& playlist = m_browser.playlist();
        m_playback.play(PlaybackPage{playlist.tracks, playlist.continuation}, start, m_item.endpoint.browse_id);
        return;
    }

    if (queue.position() == start) {
        m_playback.toggle_pause();
        return;
    }

    queue.select(queue.entries()[start].id);
}

void PlaylistOverlay::update_playback() {
    if (m_playback.queue().source() != m_item.endpoint.browse_id) {
        m_play->set_text("Play");
        return;
    }

    m_play->set_text(m_playback.paused() ? "Resume" : "Pause");
}

void PlaylistOverlay::load_more() {
    m_browser.load_more();
    refresh();
}

void PlaylistOverlay::refresh() {
    const auto& playlist = m_browser.playlist();
    m_title->set_text(playlist.title.empty() ? m_item.title : playlist.title);
    m_author->set_text(playlist.author.text.empty() ? media_artist(m_item) : playlist.author.text);

    auto details = playlist.year;
    if (!details.empty() && !playlist.duration.empty()) {
        details += " · ";
    }
    details += playlist.duration;
    m_details->set_text(std::move(details));
    m_details->set_visible(!m_details->empty());

    m_description->set_text(playlist.description);
    m_description->set_visible(!m_description->empty());

    m_play->set_enabled(!playlist.tracks.empty());
    update_playback();
    m_tracks->set_items(playlist.tracks.size());

    m_status->set_text(m_browser.loading() ? "Loading tracks..." : "");
    m_status->set_visible(!m_status->empty());

    m_more->set_visible(playlist.continuation.has_value());
    m_more->set_enabled(!m_browser.loading());
}

void PlaylistOverlay::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    configure_all_styles([](Style& style) { style.padding({24.0f, 16.0f, 24.0f, 16.0f}); });
}
