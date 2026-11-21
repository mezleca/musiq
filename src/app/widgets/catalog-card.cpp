#include "catalog-card.hpp"
#include "text.hpp"
#include "media-cover.hpp"
#include "placeholder.hpp"
#include "../../utils/media.hpp"
#include "../../youtube/schema/common.hpp"
#include <arbor/surface.hpp>

using namespace ui;
using namespace utils::media;

static std::string playlist_subtitle(const MusicItem& item) {
    const bool album = is_album(item.endpoint);
    auto type = subtitle_part(item);
    if (type != "EP" && type != "Single") type = album ? "Album" : "Playlist";

    const auto artist = media_artist(item);
    return artist.empty() ? type : type + " by " + artist;
}

static void add_card_text(Container& container, std::string text, FontWeight weight, std::string id, bool loading) {
    MusiqText* label;

    if (loading) {
        label = &container.add<PlaceholderText>(weight);
    } else {
        label = &container.add<MusiqText>(std::move(text), weight);
    }

    label->set_id(std::move(id));
    label->set_size({grow(), fit()});
    label->set_overflow(TextOverflow::Ellipsis);
}

static void add_card_cover(Container& container, const MusicItem& item, ThumbnailCache& thumbnails, float size, bool loading) {
    if (loading) {
        container.add<PlaceholderCover>(size);
    } else {
        container.add<MediaCoverWidget>(thumbnails, size, item.thumbnails);
    }
}

MediaCardWidget::MediaCardWidget(const MusicItem& item, ThumbnailCache& thumbnails, bool loading, std::size_t number)
    : MusiqButtonWidget("", {grow(), fit()}), m_numbered(number != 0) {
    set_type_name("MediaCardWidget");
    set_direction(StackDirection::Horizontal);
    set_spacing(10.0f);
    set_content_alignment(Anchor::CenterLeft);
    set_enabled(!loading);

    if (m_numbered) {
        auto& index = add<MusiqText>(std::to_string(number), FontWeight::Regular);
        index.set_id("index");
        index.set_size({px(28), fit()});
        index.set_alignment({0.5f, 0.5f});
    }

    add_card_cover(*this, item, thumbnails, m_numbered ? 44.0f : 48.0f, loading);

    auto& details = add<Container>("details", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(m_numbered ? 2.0f : 4.0f);

    add_card_text(details, item.title, FontWeight::Bold, "title", loading);
    add_card_text(details, media_artist(item), FontWeight::Regular, "artist", loading);
}

void MediaCardWidget::apply_theme_defaults(const Theme& theme) {
    MusiqButtonWidget::apply_theme_defaults(theme);

    configure_all_styles([this, &theme](Style& style) {
        style.padding(BoxInsets{8.0f, m_numbered ? 6.0f : 8.0f})
            .background_color(theme.transparent)
            .border_radius(theme.controls.rounding)
            .border_thickness(0.0f);
    });

    style(StyleType::HOVER).background_color(theme.controls.hover_color);
    style(StyleType::ACTIVE).background_color(theme.controls.hover_color);
    static_cast<MusiqText*>(find("title"))->configure_all_styles([this, &theme](Style& style) {
        style.color(m_selected ? theme.accent_color : theme.text_color);
    });
}

void MediaCardWidget::set_selected(bool selected) {
    if (m_selected == selected) return;
    m_selected = selected;
    apply_theme_defaults(surface().theme());
}

class PlaylistCoverOverlay final : public Container {
public:
    explicit PlaylistCoverOverlay(Widget& owner) : Container("controls"), m_owner(owner) {
        set_layout({.size = {grow(), grow()}, .in_flow = false});
    }

    void draw() override {
        const auto input = m_owner.subtree_input_state();
        if (input.hovered || input.active) Container::draw();
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        Container::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) {
            style.padding({6.0f})
                .border_radius(theme.controls.rounding)
                .background_color(gradient(
                    GradientType::Linear, {{0.0f, rgba(0, 0, 0, 0)}, {1.0f, rgba(0, 0, 0, 89)}}, {0.0f, 0.0f}, {1.0f, 0.0f}
                ));
        });
    }

private:
    Widget& m_owner;
};

class PlaylistPlayButton final : public MusiqIconButtonWidget {
public:
    explicit PlaylistPlayButton(Widget& owner) : MusiqIconButtonWidget("icons/play.svg", 24.0f), m_owner(owner) {}

    void draw() override {
        if (m_owner.subtree_input_state().hovered) MusiqIconButtonWidget::draw();
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        MusiqIconButtonWidget::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) { style.padding({}).background_color(theme.transparent); });
    }

private:
    Widget& m_owner;
};

PlaylistCardWidget::PlaylistCardWidget(
    const MusicItem& item, ThumbnailCache& thumbnails, OpenMenu menu, std::function<void()> play
)
    : MusiqButtonWidget("", {px(176), fit()}) {
    set_type_name("PlaylistCardWidget");
    set_direction(StackDirection::Vertical);
    set_spacing(8.0f);
    auto& cover = add<Container>("cover");
    cover.set_size({px(176), px(176)});
    cover.add<MediaCoverWidget>(thumbnails, 176.0f, item.thumbnails);
    auto& overlay = cover.add<PlaylistCoverOverlay>(*this);
    if (menu) attach_context_menu(*this, std::move(menu), &overlay);
    if (play) {
        auto& button = overlay.add<PlaylistPlayButton>(*this);
        button.set_layout({
            .size = {px(24), px(24)},
            .placement = {.anchor = Anchor::BottomRight, .origin = Anchor::BottomRight, .offset = {}},
            .in_flow = false,
        });
        button.on_event(EventType::Click, [play = std::move(play)](UiEvent& event) {
            play();
            event.stop_propagation();
        });
    }

    auto& details = add<Container>("details", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(4.0f);
    add_card_text(details, item.title, FontWeight::Bold, "title", false);
    add_card_text(details, playlist_subtitle(item), FontWeight::Regular, "metadata", false);
}

void PlaylistCardWidget::apply_theme_defaults(const Theme& theme) {
    MusiqButtonWidget::apply_theme_defaults(theme);

    configure_all_styles([&theme](Style& style) {
        style.padding({}).background_color(theme.transparent).border_thickness(0.0f);
    });
}
