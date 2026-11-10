#include "catalog-card.hpp"
#include "text.hpp"
#include "media-cover.hpp"
#include "../../utils/media.hpp"
#include "../../youtube/schema/common.hpp"
#include <imgui-ui/surface.hpp>

using namespace ui;
using namespace utils::media;

static std::string playlist_subtitle(const MusicItem& item) {
    const bool album = is_album(item.endpoint);
    auto type = subtitle_part(item);
    if (type != "EP" && type != "Single") type = album ? "Album" : "Playlist";

    const auto artist = media_artist(item);
    return artist.empty() ? type : type + " by " + artist;
}

static void add_card_text(Container& container, std::string text, TextWeight weight, std::string id) {
    auto& label = container.add<MusiqText>(std::move(text), weight);
    label.set_id(std::move(id));
    label.set_size({grow(), fit()});
    label.set_overflow(TextOverflow::Ellipsis);
}

MediaCardWidget::MediaCardWidget(const MusicItem& item, ThumbnailCache& thumbnails) : MusiqButtonWidget("", {grow(), fit()}) {
    set_type_name("MediaCardWidget");
    set_direction(StackDirection::Horizontal);
    set_spacing(10.0f);
    set_content_alignment(Anchor::CenterLeft);
    add<MediaCoverWidget>(item, thumbnails, 48.0f);

    auto& details = add<Container>("details", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(4.0f);
    add_card_text(details, item.title, TextWeight::Bold, "title");
    add_card_text(details, media_artist(item), TextWeight::Regular, "artist");
}

void MediaCardWidget::apply_theme_defaults(const Theme& theme) {
    MusiqButtonWidget::apply_theme_defaults(theme);
    configure_all_styles([this, &theme](Style& style) {
        style.padding({8.0f})
            .background_color(m_selected ? theme.accent_color : theme.background_secondary_color)
            .border_radius(theme.controls.rounding)
            .border_thickness(0.0f);
    });
    style(StyleType::HOVER).background_color(m_selected ? theme.accent_hover_color : theme.controls.hover_color);
}

void MediaCardWidget::set_selected(bool selected) {
    if (m_selected == selected) return;
    m_selected = selected;
    apply_theme_defaults(surface().theme());
}

PlaylistCardWidget::PlaylistCardWidget(const MusicItem& item, ThumbnailCache& thumbnails)
    : MusiqButtonWidget("", {px(176), fit()}) {
    set_type_name("PlaylistCardWidget");
    set_direction(StackDirection::Vertical);
    set_spacing(8.0f);
    add<MediaCoverWidget>(item, thumbnails, 176.0f);

    auto& details = add<Container>("details", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(4.0f);
    add_card_text(details, item.title, TextWeight::Bold, "title");
    add_card_text(details, playlist_subtitle(item), TextWeight::Regular, "metadata");
}

void PlaylistCardWidget::apply_theme_defaults(const Theme& theme) {
    MusiqButtonWidget::apply_theme_defaults(theme);
    configure_all_styles([&theme](Style& style) {
        style.padding({}).background_color(theme.transparent).border_thickness(0.0f);
    });
}
