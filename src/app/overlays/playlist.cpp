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
    std::function<void(const MusicPlaylist&)> edit_playlist, MediaMenu menu
)
    : Container("playlist", StackDirection::Vertical), m_item(std::move(item)), m_thumbnails(thumbnails), m_playback(playback),
      m_edit_playlist(std::move(edit_playlist)), m_menu(std::move(menu)) {
    set_type_name("PlaylistOverlay");
    set_size({grow(), grow()});
    set_spacing(16.0f);
    set_scrollable(true);
    set_visible(false);

    auto& header = add<PlaylistDetailsWidget>();
    m_header = &header;
    m_cover = &header.add<MediaCoverWidget>(m_thumbnails, 180.0f, m_item.thumbnails, true);

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

    m_separator = &add<Container>("separator");
    m_separator->set_size({grow(), px(1)});

    m_tracks = &add<TrackList>();
    m_tracks->set_size({grow(), fit()});
    m_tracks->set_scrollable(false);
    m_tracks->set_item_height(56.0f);
    m_tracks->set_spacing(2.0f);
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
                return m_menu(
                    current.tracks[index],
                    {.playlist_id = current.owned ? current.id : std::string_view{}, .queue_entry_id = 0, .library_entry = false},
                    position
                );
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
    m_recommended_tracks->set_item_height(56.0f);
    m_recommended_tracks->set_spacing(2.0f);
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

void PlaylistOverlay::load(const YoutubeMusicSession& session, PlaylistLoad mode) {
    if (!m_browser.load(session, m_item, mode)) {
        update_playback();
        return;
    }

    m_tracks->clear_tracks();
    m_recommended_tracks->clear_tracks();
    scroll().seek_to({}, ScrollBehavior::Instant);

    refresh();
}

PlaylistChange PlaylistOverlay::poll() {
    const auto change = m_browser.poll(m_item);
    if (change == PlaylistChange::Loaded) {
        const auto& playlist = m_browser.playlist();
        m_cover->set_thumbnails(playlist.thumbnails.empty() ? m_item.thumbnails : playlist.thumbnails);
    }

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

void PlaylistOverlay::update_playback() {
    const auto& queue = m_playback.queue();
    const bool paused = !queue.matches_prefix(m_browser.playlist().tracks, m_item.endpoint.browse_id) || m_playback.paused();
    m_play->set_icon(paused ? "icons/play.svg" : "icons/pause.svg");
}

void PlaylistOverlay::refresh() {
    const auto& playlist = m_browser.playlist();
    const bool loading = m_browser.loading() && !m_browser.appending();
    m_header->set_visible(!loading);
    m_separator->set_visible(!loading);

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

    m_tracks->set_items(loading ? 0 : playlist.tracks.size());

    m_status->set_text(loading ? "Loading..." : m_browser.error());
    m_status->set_visible(!m_status->empty());

    const bool recommendations_loading = m_browser.recommendations_loading();
    const auto& recommendations_error = m_browser.recommendations_error();
    m_recommended_section->set_visible(
        !loading && !playlist.continuation &&
        (recommendations_loading || !m_browser.recommendations().empty() || !recommendations_error.empty())
    );
    m_recommended_status->set_text(recommendations_loading ? "Loading..." : recommendations_error);
    m_recommended_status->set_visible(!m_recommended_status->empty());
    m_recommended_tracks->set_items(m_browser.recommendations().size());
}

void PlaylistOverlay::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    configure_all_styles([](Style& style) { style.padding({12.0f, 0.0f}); });
    m_separator->configure_all_styles([&theme](Style& style) {
        style.background_color(theme.transparent).border_top(1.0f).border_color(theme.border_color).border_radius(0.0f);
    });
    m_recommended_section->configure_all_styles([&theme](Style& style) {
        style.padding({12.0f, 20.0f, 12.0f, 0.0f}).border_top(1.0f).border_color(theme.border_color).border_radius(0.0f);
    });
}
