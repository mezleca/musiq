#include "playlist.hpp"
#include "../../utils/media.hpp"
#include "../widgets/catalog-card.hpp"
#include "../widgets/text.hpp"
#include "../widgets/media-cover.hpp"
#include "../../playback/controller.hpp"

#include <utility>
#include <algorithm>
#include <imgui-ui/layout/virtual-layout.hpp>
#include <imgui-ui/surface.hpp>

using namespace ui;
using namespace utils::media;

class PlaylistPlayButtonWidget final : public MusiqIconButtonWidget {
public:
    PlaylistPlayButtonWidget() : MusiqIconButtonWidget("icons/play.svg", 42.0f) {}

protected:
    void apply_theme_defaults(const Theme& theme) override {
        MusiqIconButtonWidget::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) { style.background_color(theme.controls.hover_color); });
    }
};

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

    m_play = &details.add<PlaylistPlayButtonWidget>();
    m_play->set_tooltip("Play");
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
    m_list.emplace(*m_tracks);
    m_tracks->set_items(0, [this](std::size_t index) -> Node& {
        if (auto* card = m_list->find(index)) return *card;

        auto& card = m_list->add(index, m_browser.playlist().tracks[index], m_thumbnails);
        card.on_click([this, index] { play(index); });
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

    m_list->clear();
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
    const auto& queue = m_playback.queue();
    if (queue.source() != m_item.endpoint.browse_id) {
        m_play->set_icon("icons/play.svg");
        m_play->set_tooltip("Play");
        m_list->set_current(std::nullopt);
        return;
    }

    const bool paused = m_playback.paused();
    m_play->set_icon(paused ? "icons/play.svg" : "icons/pause.svg");
    m_play->set_tooltip(paused ? "Play" : "Pause");

    std::optional<std::uint64_t> current;

    const auto* entry = queue.current();
    const auto& tracks = m_browser.playlist().tracks;
    if (entry) {
        // the queue was built from this playlist, so its position also identifies the exact occurrence
        const auto position = queue.position();
        if (position < tracks.size() && tracks[position].endpoint.video_id == entry->media.endpoint.video_id) {
            current = static_cast<std::uint64_t>(position);
        } else {
            const auto match = std::ranges::find_if(tracks, [&](const MusicItem& track) {
                return track.endpoint.video_id == entry->media.endpoint.video_id;
            });
            if (match != tracks.end()) current = static_cast<std::uint64_t>(match - tracks.begin());
        }
    }

    m_list->set_current(current);
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
