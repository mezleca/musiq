#include "media-cover.hpp"
#include "spinner.hpp"
#include "../resources/thumbnails.hpp"
#include "../../utils/media.hpp"

#include <arbor/runtime.hpp>
#include <arbor/surface.hpp>
#include <algorithm>

using namespace ui;
using namespace utils::media;

MediaCoverWidget::MediaCoverWidget(
    ThumbnailCache& thumbnails, float size, std::span<const MusicThumbnail> sources, bool preserve_aspect
)
    : m_thumbnails(thumbnails), m_size(size), m_preserve_aspect(preserve_aspect) {
    set_type_name("MediaCoverWidget");
    set_size({px(size), px(size)});
    set_fit(preserve_aspect ? ImageFit::Contain : ImageFit::Cover);

    set_thumbnails(sources);
}

void MediaCoverWidget::set_thumbnails(std::span<const MusicThumbnail> thumbnails) {
    const auto* source = select_thumbnail(thumbnails, m_size);
    const std::string_view url = source ? std::string_view(source->url) : std::string_view{};
    if (url == m_url) return;

    m_url = url;
    m_download = {};
    m_loaded = false;

    // keep the previous artwork visible until its replacement finishes decoding
    if (m_url.empty()) {
        set_texture(nullptr);
        m_has_texture = false;
    }
}

std::optional<Color> MediaCoverWidget::accent() const {
    return m_thumbnails.accent(m_url);
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

    if (m_preserve_aspect && image) {
        const auto size = image->size();
        const float scale = m_size / std::max(size.x, size.y);
        set_size({px(size.x * scale), px(size.y * scale)});
    }

    m_has_texture = static_cast<bool>(image);
    set_texture(std::move(image));
    m_loaded = true;
}

void MediaCoverWidget::paint_content(const PaintContext& context) {
    ImageWidget::paint_content(context);
    if (m_loaded || m_has_texture || m_url.empty()) {
        return;
    }

    const auto rect = context.painter.to_screen(context.content_rect);
    if (!ImGui::IsRectVisible(rect.min, rect.max)) {
        return;
    }

    paint_spinner(context.painter, context.content_rect, surface().theme().text_color);
}
