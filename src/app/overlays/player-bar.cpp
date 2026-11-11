#include "player-bar.hpp"
#include "../../utils/media.hpp"
#include "../widgets/text.hpp"
#include "../widgets/media-cover.hpp"
#include "../widgets/spinner.hpp"
#include "../widgets/slider.hpp"
#include "../../playback/queue.hpp"
#include "../../playback/mpv-player.hpp"
#include "../../playback/controller.hpp"
#include "../../database/settings.hpp"
#include <imgui-ui/surface.hpp>

#include <utility>
#include <format>
#include <cmath>

using namespace ui;
using namespace utils::media;

static std::string playback_time(double seconds) {
    const auto total = static_cast<int>(std::max(0.0, seconds));
    return std::format("{}:{:02}", total / 60, total % 60);
}

QueueButtonWidget::QueueButtonWidget() : MusiqIconButtonWidget("icons/queue.svg", 44.0f) {
    set_layout(
        {.size = {px(44), px(44)},
         .placement = {.anchor = Anchor::TopRight, .origin = Anchor::BottomRight, .offset = {-16.0f, -12.0f}},
         .in_flow = false}
    );
    set_tooltip("Open queue");
}

void QueueButtonWidget::apply_theme_defaults(const Theme& theme) {
    MusiqIconButtonWidget::apply_theme_defaults(theme);
    configure_all_styles([&theme](Style& style) {
        style.background_color(theme.background_secondary_color)
            .border_radius(22.0f)
            .border_thickness(1.0f)
            .border_color(theme.border_color);
    });
    style(StyleType::HOVER).background_color(theme.controls.hover_color);
}

PlayerBarOverlay::PlayerBarOverlay(
    ThumbnailCache& thumbnails, MpvPlayer& player, PlaybackController& playback, std::function<void(int)> volume_changed
)
    : Container("player-bar", StackDirection::Horizontal), m_player(player), m_playback(playback) {
    set_type_name("PlayerBarOverlay");
    set_size({grow(), px(104)});
    set_content_alignment(Anchor::CenterLeft);

    auto& left = add<Container>("left", StackDirection::Horizontal);
    left.set_size({grow(), grow()});
    left.set_content_alignment(Anchor::CenterLeft);

    m_selection = &left.add<Container>("selected-track", StackDirection::Horizontal);
    m_selection->set_size({grow(), fit()});
    m_selection->set_content_alignment(Anchor::CenterLeft);
    m_selection->set_spacing(10.0f);
    m_selection->set_visible(false);
    m_cover = &m_selection->add<MediaCoverWidget>(MusicItem{}, thumbnails, 64.0f);

    auto& details = m_selection->add<Container>("metadata", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(4.0f);
    m_title = &details.add<MusiqText>("", FontWeight::Bold);
    m_artist = &details.add<MusiqText>("");
    m_status = &details.add<MusiqText>("");
    m_status->set_visible(false);
    for (auto* text : {m_title, m_artist, m_status}) {
        text->set_size({grow(), fit()});
        text->set_overflow(TextOverflow::Ellipsis);
    }

    auto& center = add<Container>("timeline", StackDirection::Vertical);
    center.set_size({grow(2.0f), grow()});
    center.set_spacing(8.0f);
    center.set_content_alignment(Anchor::Center);

    auto& controls = center.add<Container>("controls", StackDirection::Horizontal);
    controls.set_size({fit(), fit()});
    controls.set_content_alignment(Anchor::Center);
    controls.set_spacing(8.0f);

    m_random = &controls.add<MusiqIconButtonWidget>("icons/shuffle.svg");
    m_random->set_tooltip("Random");
    m_random->set_enabled(false);
    m_random->on_click([this] { m_playback.queue().set_random(!m_playback.queue().random()); });

    m_previous = &controls.add<MusiqIconButtonWidget>("icons/skip-previous.svg");
    m_previous->set_tooltip("Previous track");
    m_previous->on_click([this] { m_playback.queue().previous(); });
    m_previous->set_enabled(false);

    m_play = &controls.add<MusiqIconButtonWidget>("icons/play.svg", 42.0f);
    m_play->on_click([this] { m_playback.toggle_pause(); });
    m_play->set_enabled(false);
    m_spinner = &m_play->add<SpinnerWidget>();
    m_spinner->set_layout(
        {.size = {px(24), px(24)},
         .placement = {.anchor = Anchor::Center, .origin = Anchor::Center, .offset = {}},
         .in_flow = false}
    );
    m_spinner->set_visible(false);

    m_next = &controls.add<MusiqIconButtonWidget>("icons/skip-next.svg");
    m_next->set_tooltip("Next track");
    m_next->on_click([this] { m_playback.queue().advance(); });
    m_next->set_enabled(false);

    m_repeat = &controls.add<MusiqIconButtonWidget>("icons/repeat.svg");
    m_repeat->set_enabled(false);
    m_repeat->set_tooltip("Repeat this track once");
    m_repeat->on_click([this] { m_playback.queue().set_repeat(!m_playback.queue().repeat()); });

    auto& progress = center.add<Container>("progress", StackDirection::Horizontal);
    progress.set_size({percent(75), fit()});
    progress.set_content_alignment(Anchor::CenterLeft);
    progress.set_spacing(6.0f);
    m_position = &progress.add<MusiqText>("0:00");

    auto& seek = progress.add<MusiqSliderWidget>(m_progress);
    seek.set_commit_on_release(true);
    seek.on_change([this] { m_player.seek(m_progress * m_player.state().duration / 100.0); });
    seek.set_enabled(false);
    m_seek = &seek;
    m_duration = &progress.add<MusiqText>("0:00");

    auto& right = add<Container>("volume", StackDirection::Horizontal);
    right.set_size({grow(), grow()});
    right.set_content_alignment(Anchor::CenterRight);
    right.add<MusiqIconButtonWidget>("icons/volume.svg").set_input_enabled(false);

    m_volume = MusiqSettings::instance().volume();
    m_player.set_volume(static_cast<int>(m_volume));
    auto& volume = right.add<MusiqSliderWidget>(m_volume);
    volume.set_size({px(100), px(20)});
    volume.on_change([this, volume_changed = std::move(volume_changed)] {
        volume_changed(static_cast<int>(std::lround(m_volume)));
    });
}

void PlayerBarOverlay::refresh() {
    const auto& queue = m_playback.queue();
    const auto* current = queue.current();
    const auto id = current ? current->id : 0;
    if (m_current_id != id) {
        m_current_id = id;
        m_selection->set_visible(current != nullptr);
        if (current) {
            m_title->set_text(current->media.title);
            m_artist->set_text(media_artist(current->media));
            m_cover->set_item(current->media);
        }
        m_duration->set_text(current && !current->media.duration.empty() ? current->media.duration : "0:00");
        m_position_seconds = -1;
        m_duration_seconds = -1;
    }

    m_previous->set_enabled(queue.position() > 0);
    m_next->set_enabled(queue.can_advance());
    m_random->set_enabled(queue.entries().size() > 1);
    m_random->set_selected(queue.random());
    m_repeat->set_enabled(current != nullptr);
    m_repeat->set_selected(queue.repeat());

    const auto& state = m_player.state();
    const bool loading = m_playback.resolving() || state.buffering;
    const bool playing = state.loaded && !state.paused;

    m_spinner->set_visible(loading);
    m_play->set_icon(loading ? "" : playing ? "icons/pause.svg" : "icons/play.svg");
    m_play->set_enabled(current && !loading);
    m_play->set_tooltip(playing ? "Pause" : "Play");
    m_seek->set_enabled(state.loaded && state.duration > 0);

    if (!m_seek->dragging()) m_progress = state.duration > 0 ? state.position * 100.0 / state.duration : 0;

    // format only when the displayed whole second changes
    const int position_seconds = static_cast<int>(std::max(0.0, state.position));
    if (position_seconds != m_position_seconds) {
        m_position_seconds = position_seconds;
        m_position->set_text(playback_time(state.position));
    }
    const int duration_seconds = static_cast<int>(state.duration);
    if (duration_seconds > 0 && duration_seconds != m_duration_seconds) {
        m_duration_seconds = duration_seconds;
        m_duration->set_text(playback_time(state.duration));
    }

    const auto& error = m_playback.playback_error().empty() ? state.error : m_playback.playback_error();
    m_status->set_text(error);
    m_status->set_visible(!error.empty());
    m_status->set_tooltip(error);
}

void PlayerBarOverlay::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);

    configure_all_styles([&theme](Style& style) {
        style.background_color(theme.background_tertiary_color);
        style.padding({20.0f, 12.0f, 20.0f, 12.0f});
        style.border_top(1.0f);
        style.border_radius(0.0f);
        style.border_color(theme.border_color);
    });
}
