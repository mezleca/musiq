#include "playlist.hpp"
#include "../../utils/media.hpp"
#include "../widgets/catalog-card.hpp"
#include "../widgets/text.hpp"
#include "../widgets/media-cover.hpp"
#include "../../playback/controller.hpp"

#include <algorithm>
#include <utility>
#include <arbor/surface.hpp>

using namespace ui;
using namespace utils::media;

static constexpr float TrackHeight = 56.0f;
static constexpr float TrackSpacing = 2.0f;
static constexpr float PlaylistSpacing = 16.0f;

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

PlaylistOverlay::PlaylistOverlay(
    MusicItem item, ThumbnailCache& thumbnails, PlaybackController& playback,
    std::function<void(const MusicPlaylist&)> edit_playlist, MediaMenu menu
)
    : Container("playlist", StackDirection::Vertical), m_item(std::move(item)), m_thumbnails(thumbnails), m_playback(playback),
      m_edit_playlist(std::move(edit_playlist)), m_menu(std::move(menu)) {
    set_type_name("PlaylistOverlay");
    set_size({grow(), grow()});
    set_spacing(PlaylistSpacing);
    set_scrollable(true);
    set_visible(false);

    m_backdrop = &add<Container>("playlist-backdrop");
    m_backdrop->set_layout({.in_flow = false});
    m_backdrop->set_input_mode(InputMode::None);

    auto& header = add<Container>("details", StackDirection::Horizontal);
    header.set_size({grow(), fit()});
    header.set_spacing(24.0f);
    header.set_content_alignment(Anchor::CenterLeft);
    header.configure_all_styles([](Style& style) { style.padding({20.0f, 28.0f, 20.0f, 14.0f}); });
    m_header = &header;
    m_cover = &header.add<MediaCoverWidget>(m_thumbnails, 180.0f, m_item.thumbnails, true);

    auto& details = header.add<Container>("info", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(8.0f);
    details.configure_all_styles([](Style& style) { style.padding_right(50.0f); });
    m_title = &details.add<MusiqText>(m_item.title, FontWeight::Bold, 30.0f);
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
    m_play->set_enabled(false);
    m_play->on_click([this] {
        if (m_playback.queue().matches_prefix(m_browser.playlist().tracks, m_item.endpoint.browse_id)) {
            m_playback.toggle_pause();
        } else {
            play(0);
        }
    });

    m_edit = &header.add<PlaylistControlWidget>("icons/edit.svg");
    m_edit->set_layout({
        .size = {px(42), px(42)},
        .placement = {.anchor = Anchor::TopRight, .origin = Anchor::TopRight, .offset = {}},
        .in_flow = false,
    });
    m_edit->set_visible(false);
    m_edit->on_click([this] {
        if (m_edit_playlist) m_edit_playlist(m_browser.playlist());
    });

    m_status = &add<MusiqText>("");

    m_tracks = &add<TrackList>();
    m_tracks->set_size({grow(), fit()});
    m_tracks->set_scrollable(false);
    m_tracks->set_item_height(TrackHeight);
    m_tracks->set_spacing(TrackSpacing);
    m_tracks->set_overscan(2);
    m_tracks->set_items(0, [this](std::size_t index) -> Node& {
        const auto& playlist = m_browser.playlist();
        if (index + 8 >= playlist.tracks.size()) {
            m_browser.load_more();
        }

        if (auto* card = m_tracks->find_track(index)) {
            return *card;
        }

        const auto& item = playlist.tracks[index];
        auto& card = m_tracks->add_track(index, item, m_thumbnails, false, index + 1);
        card.on_click([this, index] { play(index); });
        if (m_menu) {
            attach_context_menu(card, [this, index](ImVec2 position) {
                const auto& current = m_browser.playlist();
                if (index >= current.tracks.size()) return false;
                return m_menu(current.tracks[index], {.playlist_id = editable() ? current.id : std::string_view{}}, position);
            });
        }

        return card;
    });

    m_recommended_section = &add<Container>("recommended", StackDirection::Vertical);
    m_recommended_section->set_size({grow(), fit()});
    m_recommended_section->set_spacing(12.0f);

    auto& recommended_title = m_recommended_section->add<MusiqText>("Recommended", FontWeight::Semibold);
    recommended_title.configure_all_styles([](Style& style) { style.padding({8.0f, 0.0f}); });

    m_recommended_status = &m_recommended_section->add<MusiqText>("");
    m_recommended_tracks = &m_recommended_section->add<TrackList>();
    m_recommended_tracks->set_size({grow(), fit()});
    m_recommended_tracks->set_scrollable(false);
    m_recommended_tracks->set_item_height(TrackHeight);
    m_recommended_tracks->set_spacing(TrackSpacing);
    m_recommended_tracks->set_items(0, [this](std::size_t index) -> Node& {
        if (auto* card = m_recommended_tracks->find_track(index)) {
            return *card;
        }

        const auto& item = m_browser.recommendations()[index];
        auto& card = m_recommended_tracks->add_track(index, item, m_thumbnails);
        card.on_click([this, index] { m_playback.play(m_browser.recommendations()[index]); });

        if (m_menu) {
            attach_context_menu(card, [this, index](ImVec2 position) {
                const auto& recommendations = m_browser.recommendations();
                if (index >= recommendations.size()) return false;
                return m_menu(recommendations[index], {}, position);
            });
        }

        return card;
    });

    m_recommended_section->set_visible(false);
}

void PlaylistOverlay::invalidate() {
    m_browser.invalidate();
}

bool PlaylistOverlay::append_added_track(MusicItem track) {
    if (!m_browser.append_added_track(std::move(track))) return false;
    refresh();
    return true;
}

void PlaylistOverlay::remove_track(std::string_view set_video_id) {
    const auto index = m_browser.remove_track(set_video_id);
    if (!index) return;

    m_tracks->retain([index = *index](std::uint64_t row) { return row < index; });
    m_tracks->set_items(m_browser.playlist().tracks.size());
    update_playback();
}

void PlaylistOverlay::update_cover(const std::vector<MusicThumbnail>& thumbnails) {
    m_browser.set_thumbnails(thumbnails);
    m_cover->set_thumbnails(thumbnails.empty() ? m_item.thumbnails : thumbnails);
}

bool PlaylistOverlay::editable() const {
    return playlist().owned && !is_liked_music_playlist(m_item.endpoint.browse_id);
}

void PlaylistOverlay::load(const YoutubeMusicSession& session, PlaylistLoad mode) {
    if (!m_browser.load(session, m_item, mode)) {
        update_playback();
        return;
    }

    if (mode != PlaylistLoad::Reload) {
        m_tracks->clear_tracks();
        m_recommended_tracks->clear_tracks();
        scroll().seek_to({}, ScrollBehavior::Instant);
        m_play->set_enabled(false);
    }
}

PlaylistChange PlaylistOverlay::poll() {
    const auto change = m_browser.poll(m_item);
    if (change == PlaylistChange::Loaded) m_tracks->clear_tracks();

    if (change != PlaylistChange::None) refresh();

    return change;
}

void PlaylistOverlay::play(std::size_t start) {
    const auto& playlist = m_browser.playlist();
    auto& queue = m_playback.queue();
    if (!queue.matches_prefix(playlist.tracks, m_item.endpoint.browse_id)) {
        m_playback.play_playlist(
            PlaybackPage{playlist.tracks, playlist.continuation}, start, m_item.endpoint.browse_id, !is_album(m_item.endpoint)
        );
        return;
    }

    if (queue.position() == start) {
        return;
    }

    queue.select(queue.entries()[start].id);
}

void PlaylistOverlay::show_action_error(std::string message) {
    m_status->set_text(std::move(message));
    m_status->set_visible(!m_status->empty());
}

void PlaylistOverlay::reveal_track(const MusicItem& track, ScrollBehavior behavior) {
    m_track_reveal = {track.endpoint.video_id, track.playlist_set_video_id, behavior};
}

void PlaylistOverlay::draw_children() {
    const auto scale = dpi_scale();
    auto backdrop_height = m_header->layout().intrinsic_size().y / scale + PlaylistSpacing + TrackHeight;
    if (m_status->visible()) backdrop_height += m_status->layout().intrinsic_size().y / scale + PlaylistSpacing;
    m_backdrop->set_size({grow(), px(backdrop_height)});

    if (const auto accent = m_cover->accent(); accent && accent != m_cover_accent) update_backdrop();

    Container::draw_children();

    if (!m_track_reveal || (m_browser.loading() && !m_browser.appending())) return;

    const auto& target = *m_track_reveal;
    const auto& tracks = m_browser.playlist().tracks;
    const auto track = std::ranges::find_if(tracks, [&target](const MusicItem& item) {
        return item.endpoint.video_id == target.video_id &&
               (target.set_video_id.empty() || item.playlist_set_video_id == target.set_video_id);
    });
    if (track == tracks.end()) {
        if (!m_browser.appending() && !m_browser.load_more()) m_track_reveal.reset();
        return;
    }

    const auto index = static_cast<float>(track - tracks.begin());
    const auto list_top = m_tracks->layout().visual_rect().min.y;
    const auto viewport = layout().visual_rect();
    const auto row_center = list_top + (index * (TrackHeight + TrackSpacing) + TrackHeight / 2.0f) * dpi_scale();
    const auto viewport_center = (viewport.min.y + viewport.max.y) / 2.0f;
    scroll().seek_to({0.0f, scroll().position().y + row_center - viewport_center}, target.behavior);

    m_track_reveal.reset();
}

void PlaylistOverlay::update_playback() {
    const auto& queue = m_playback.queue();
    const bool paused = !queue.matches_prefix(m_browser.playlist().tracks, m_item.endpoint.browse_id) || m_playback.paused();
    m_play->set_icon(paused ? "icons/play.svg" : "icons/pause.svg");
}

void PlaylistOverlay::refresh() {
    const auto& playlist = m_browser.playlist();
    const bool loading = m_browser.loading() && !m_browser.appending();

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
    m_edit->set_visible(editable());
    update_playback();

    m_tracks->set_items(playlist.tracks.size());

    m_status->set_text(loading ? "Loading..." : m_browser.error());
    m_status->set_visible(!m_status->empty());

    const auto& recommendations_error = m_browser.recommendations_error();
    m_recommended_section->set_visible(
        !loading && !playlist.continuation && (!m_browser.recommendations().empty() || !recommendations_error.empty())
    );
    m_recommended_status->set_text(recommendations_error);
    m_recommended_status->set_visible(!m_recommended_status->empty());
    m_recommended_tracks->set_items(m_browser.recommendations().size());
}

void PlaylistOverlay::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    configure_all_styles([](Style& style) { style.padding({12.0f, 0.0f}); });

    const auto clear_background = [&theme](Style& style) { style.background_color(theme.transparent); };
    m_header->configure_all_styles(clear_background);
    m_tracks->configure_all_styles(clear_background);

    m_cover->configure_all_styles([](Style& style) {
        style.box_shadow({.offset = {0.0f, 6.0f}, .blur = 24.0f, .color = rgba(0.0f, 0.0f, 0.0f, 0.45f)});
    });

    m_recommended_section->configure_all_styles([](Style& style) {
        style.margin({0.0f, 20.0f, 0.0f, 24.0f}).padding({12.0f, 0.0f});
    });

    update_backdrop();
}

void PlaylistOverlay::update_backdrop() {
    if (const auto accent = m_cover->accent()) m_cover_accent = accent;

    const auto& theme = surface().theme();
    const auto accent = m_cover_accent.value_or(theme.background_secondary_color);
    const auto top = interpolate_color(theme.background_color, accent, 0.55f);

    const auto color = top.rgba();
    const auto transparent = rgba(color.x, color.y, color.z, 0.0f);
    const auto background = gradient(GradientType::Linear, {{0.0f, top}, {1.0f, transparent}}, {0.0f, 0.0f}, {0.0f, 1.0f});

    m_backdrop->configure_all_styles([&background](Style& style) { style.background_color(background); });
}
