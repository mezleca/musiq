#include "media-cover.hpp"
#include "spinner.hpp"
#include "../resources/thumbnails.hpp"
#include "../../utils/media.hpp"

#include <arbor/runtime.hpp>
#include <arbor/surface.hpp>

using namespace ui;
using namespace utils::media;

MediaCoverWidget::MediaCoverWidget(const MusicItem& item, ThumbnailCache& thumbnails, float size)
    : m_thumbnails(thumbnails), m_size(size) {
    set_type_name("MediaCoverWidget");
    set_size({px(size), px(size)});
    set_fit(ImageFit::Cover);

    set_item(item);
}

void MediaCoverWidget::set_item(const MusicItem& item) {
    const auto* source = select_thumbnail(item.thumbnails, m_size);
    const auto url = source ? source->url : std::string{};
    if (url == m_url) return;

    m_url = url;
    m_download = {};
    m_loaded = false;
    set_texture(nullptr);
}

void MediaCoverWidget::apply_theme_defaults(const Theme& theme) {
    ImageWidget::apply_theme_defaults(theme);

    configure_all_styles([&theme](Style& style) {
        style.color(rgba(255, 255, 255, 255)).border_radius(theme.controls.rounding);
    });
}

void MediaCoverWidget::draw_before() {
    if (m_loaded) {
        return;
    }

    const auto rect = layout().visual_rect();
    if (!ImGui::IsRectVisible(rect.min, rect.max)) {
        return;
    }

    auto image = m_thumbnails.resolve(surface().runtime().textures(), m_url, m_download);
    if (m_download) {
        return;
    }

    set_texture(std::move(image));
    m_loaded = true;
}

void MediaCoverWidget::paint_content(const PaintContext& context) {
    ImageWidget::paint_content(context);
    if (m_loaded || m_url.empty()) return;

    const auto rect = context.painter.to_screen(context.content_rect);
    if (!ImGui::IsRectVisible(rect.min, rect.max)) return;

    paint_spinner(context.painter, context.content_rect, surface().theme().text_color);
}
