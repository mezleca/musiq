#include "playlist.hpp"
#include "../../utils/media.hpp"
#include "../widgets/catalog-card.hpp"
#include "../widgets/text.hpp"
#include "../widgets/media-cover.hpp"
#include "../../playback/controller.hpp"

#include <utility>
#include <arbor/surface.hpp>

using namespace ui;
using namespace utils::media;

class PlaylistControlWidget final : public MusiqIconButtonWidget {
public:
    explicit PlaylistControlWidget(std::string icon) : MusiqIconButtonWidget(std::move(icon), 42.0f) {}

protected:
    void apply_theme_defaults(const Theme& theme) override {
        MusiqIconButtonWidget::apply_theme_defaults(theme);

        configure_all_styles([&theme](Style& style) {
            style.background_color(theme.transparent);
            style.color(theme.accent_color);
            style.padding({4.0f, 0.0f, 4.0f, 0.0f});
        });

        style(StyleType::HOVER).color(theme.accent_hover_color);
        style(StyleType::DISABLED).color(theme.text_secondary_color);
    }
};

class PlaylistDetailsWidget final : public Container {
public:
    PlaylistDetailsWidget() : Container("details", StackDirection::Horizontal) {
        set_size({grow(), fit()});
        set_spacing(20.0f);
        set_content_alignment(Anchor::TopLeft);
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        Container::apply_theme_defaults(theme);

        configure_all_styles([&theme](Style& style) {
            style.padding({20.0f});
            style.border_radius(10.0f);
            style.background_color(gradient(
                GradientType::Linear, {{0.0f, theme.background_color}, {1.0f, theme.background_secondary_color}}, {0.0f, 0.0f},
                {0.0f, 1.0f}
            ));
        });
    }
};

PlaylistOverlay::PlaylistOverlay(
    MusicItem item, ThumbnailCache& thumbnails, PlaybackController& playback,
    std::function<void(const MusicPlaylist&)> edit_playlist
)
    : Container("playlist", StackDirection::Vertical), m_item(std::move(item)), m_thumbnails(thumbnails), m_playback(playback),
      m_edit_playlist(std::move(edit_playlist)) {
    set_type_name("PlaylistOverlay");
    set_size({grow(), grow()});
    set_spacing(16.0f);
    set_scrollable(true);
    set_visible(false);

    auto& header = add<PlaylistDetailsWidget>();
    m_cover = &header.add<MediaCoverWidget>(m_item, m_thumbnails, 180.0f);
    m_cover->set_size({{px(180), px(180)}, {px(180), px(180)}});

    auto& details = header.add<Container>("info", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(8.0f);
    details.configure_all_styles([](Style& style) { style.padding_right(50.0f); });
    m_title = &details.add<MusiqText>(m_item.title, FontWeight::Bold, 22.0f);
    m_author = &details.add<MusiqText>(media_artist(m_item));
    m_details = &details.add<MusiqText>("");
    m_description = &details.add<MusiqText>("");

    for (auto* text : {m_title, m_author, m_details, m_description}) {
        text->set_size({grow(), fit()});
        text->set_overflow(TextOverflow::Ellipsis);
    }

    auto& controls = details.add<Container>("controls", StackDirection::Horizontal);
    controls.set_size({fit(), fit()});
    controls.set_spacing(8.0f);

    m_play = &controls.add<PlaylistControlWidget>("icons/play.svg");
    m_play->on_click([this] {
        if (m_playback.queue().source() == m_item.endpoint.browse_id) {
            m_playback.toggle_pause();
        } else {
            play(0);
        }
    });

    m_edit = &header.add<PlaylistControlWidget>("icons/edit.svg");
    m_edit->set_layout({
        .size = {px(42), px(42)},
        .placement = {.anchor = Anchor::TopRight, .origin = Anchor::TopRight},
        .in_flow = false,
    });
    m_edit->set_visible(false);
    m_edit->on_click([this] {
        if (m_edit_playlist) m_edit_playlist(m_browser.playlist());
    });

    m_status = &add<MusiqText>("");

    auto& separator = add<Container>("separator");
    separator.set_size({grow(), px(1)});

    m_tracks = &add<TrackList>();
    m_tracks->set_size({grow(), fit()});
    m_tracks->set_scrollable(false);
    m_tracks->set_item_height(56.0f);
    m_tracks->set_spacing(0.0f);
    m_tracks->set_overscan(2);
    m_tracks->set_items(0, [this](std::size_t index) -> Node& {
        if (index + 8 >= m_browser.playlist().tracks.size()) {
            m_browser.load_more();
        }

        if (auto* card = m_tracks->find_track(index)) {
            return *card;
        }

        const bool loading = m_browser.loading() && !m_browser.appending();
        const MusicItem empty;
        const auto& item = loading ? empty : m_browser.playlist().tracks[index];

        auto& card = m_tracks->add_track(index, item, m_thumbnails, loading, index + 1);
        card.configure_all_styles([](Style& style) { style.padding({8.0f, 4.0f}); });

        if (!loading) {
            card.on_click([this, index] { play(index); });
        }

        return card;
    });
}

void PlaylistOverlay::invalidate() {
    m_browser.invalidate();
}

void PlaylistOverlay::load(const YoutubeMusicSession& session, bool reload) {
    if (!m_browser.load(session, m_item, reload)) {
        update_playback();
        return;
    }

    m_tracks->clear_tracks();
    m_tracks->set_items(0);
    scroll().seek_to({}, ScrollBehavior::Instant);

    m_cover->set_item(m_item);
    refresh();
}

bool PlaylistOverlay::poll() {
    const auto change = m_browser.poll();
    if (change == PlaylistChange::None) {
        return false;
    }

    const bool missing = m_browser.error() == "missing playlist tracks and header";
    const auto& playlist = m_browser.playlist();
    if (change == PlaylistChange::Loaded) {
        // loading rows share source indices with real tracks, so replace them on the first response
        m_tracks->clear_tracks();

        MusicItem cover = m_item;
        if (!playlist.thumbnails.empty()) {
            cover.thumbnails = playlist.thumbnails;
        }

        m_cover->set_item(cover);
    }

    refresh();
    return missing;
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
    const bool paused = queue.source() != m_item.endpoint.browse_id || m_playback.paused();
    m_play->set_icon(paused ? "icons/play.svg" : "icons/pause.svg");
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
    m_description->set_text(playlist.description);
    m_author->set_visible(!m_author->empty());
    m_details->set_visible(!m_details->empty());
    m_description->set_visible(!m_description->empty());

    m_play->set_enabled(!playlist.tracks.empty());
    m_edit->set_visible(playlist.owned);
    update_playback();

    // reserve track geometry without requesting artwork while the playlist loads
    m_tracks->set_items(m_browser.loading() && !m_browser.appending() ? 12 : playlist.tracks.size());

    m_status->set_text(m_browser.error());
    m_status->set_visible(!m_status->empty());
}

void PlaylistOverlay::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    configure_all_styles([](Style& style) { style.padding({24.0f, 0.0f}); });
    static_cast<Container*>(find("separator"))->configure_all_styles([&theme](Style& style) {
        style.background_color(theme.transparent).border_top(1.0f).border_color(theme.border_color).border_radius(0.0f);
    });
}
