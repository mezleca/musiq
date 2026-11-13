#include "options.hpp"
#include "text.hpp"
#include "media-cover.hpp"
#include "../../utils/path.hpp"
#include "../../utils/media.hpp"

#include <arbor/resources/texture-registry.hpp>
#include <arbor/runtime.hpp>
#include <arbor/surface.hpp>
#include <arbor/widgets/image.hpp>
#include <utility>

using namespace ui;

class OptionSubtitleWidget final : public TextWidget {
public:
    explicit OptionSubtitleWidget(std::string text) : TextWidget(std::move(text)) {
        set_size({grow(), fit()});
        set_overflow(TextOverflow::Ellipsis);
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        configure_all_styles([&theme](Style& style) { style.color(theme.text_secondary_color).font_size(14.0f); });
    }
};

static void add_label(Container& parent, std::string text, float size = 16.0f) {
    auto& label = parent.add<MusiqText>(std::move(text), FontWeight::Regular, size);
    label.set_size({grow(), fit()});
    label.set_overflow(TextOverflow::Ellipsis);
}

OptionsWidget::OptionsWidget(std::string id, std::string text, std::string icon_path, bool selected)
    : OptionsWidget(std::move(id)) {
    m_selected = selected;
    m_icon_path = std::move(icon_path);
    m_icon = &add<ImageWidget>();
    m_icon->set_size({px(24), px(24)});
    m_icon->set_fit(ImageFit::Contain);
    add_label(*this, std::move(text));
}

OptionsWidget::OptionsWidget(std::string id) : Widget(std::move(id), StackDirection::Horizontal, "OptionsWidget") {
    set_size({grow(), px(40)});
    set_content_alignment({0.0f, 0.5f});
    set_spacing(12.0f);
}

OptionsWidget::OptionsWidget(const MusicItem& playlist, ThumbnailCache& thumbnails) : OptionsWidget(playlist.endpoint.browse_id) {
    set_size({grow(), px(56)});
    add<MediaCoverWidget>(playlist, thumbnails, 40.0f);

    auto& details = add<Container>("details", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(2.0f);
    add_label(details, playlist.title, 17.0f);

    const auto creator = utils::media::media_artist(playlist);
    if (!creator.empty()) details.add<OptionSubtitleWidget>(creator);
}

void OptionsWidget::set_selected(bool selected) {
    m_selected = selected;
    apply_selection();
}

void OptionsWidget::apply_selection() {
    const auto& theme = surface().theme();
    const auto background = m_selected ? theme.header_background_color : theme.transparent;
    style(StyleType::DEFAULT).background_color(background);
    style(StyleType::FOCUS).background_color(background);
}

void OptionsWidget::apply_theme_defaults(const Theme& theme) {
    Widget::apply_theme_defaults(theme);

    configure_all_styles([this, &theme](Style& style) {
        style.padding({8.0f, 4.0f, 8.0f, 4.0f});
        style.border_thickness(0.0f);
        style.border_radius(theme.controls.rounding);
        style.background_color(m_selected ? theme.header_background_color : theme.transparent);
        style.cursor(ImGuiMouseCursor_Hand);
    });

    apply_selection();
    style(StyleType::HOVER).background_color(theme.controls.hover_color);
    style(StyleType::ACTIVE).background_color(theme.controls.active_color);

    if (!m_icon_path.empty()) {
        auto& textures = surface().runtime().textures();
        m_icon->set_texture(textures.acquire(m_icon_path, utils::path::from_resources(m_icon_path)));
        m_icon->configure_all_styles([&theme](Style& style) { style.color(theme.text_color); });
    }
}
