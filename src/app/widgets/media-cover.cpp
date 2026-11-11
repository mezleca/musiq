#include "media-cover.hpp"
#include "spinner.hpp"
#include "../resources/thumbnails.hpp"
#include "../../utils/media.hpp"

#include <imgui-ui/runtime.hpp>
#include <imgui-ui/surface.hpp>

using namespace ui;
using namespace utils::media;

MediaCoverWidget::MediaCoverWidget(const MusicItem& item, ThumbnailCache& thumbnails, float size)
    : m_thumbnails(thumbnails), m_size(size) {
    set_type_name("MediaCoverWidget");
    set_size({px(size), px(size)});
    set_fit(ImageFit::Cover);

    m_spinner = &add<SpinnerWidget>();
    m_spinner->set_layout(
        {.size = {px(24), px(24)},
         .placement = {.anchor = Anchor::Center, .origin = Anchor::Center, .offset = {}},
         .in_flow = false}
    );

    set_item(item);
}

void MediaCoverWidget::set_item(const MusicItem& item) {
    const auto* source = select_thumbnail(item.thumbnails, m_size);
    m_url = source ? source->url : std::string{};
    m_download = {};
    m_loaded = false;
    set_texture(nullptr);
    m_spinner->set_visible(!m_url.empty());
}

void MediaCoverWidget::apply_theme_defaults(const Theme& theme) {
    ImageWidget::apply_theme_defaults(theme);

    configure_all_styles([&theme](Style& style) {
        style.color(rgba(255, 255, 255, 255)).border_radius(theme.controls.rounding);
    });
}

void MediaCoverWidget::draw_before() {
    // skip if we already loaded the texture
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
    m_spinner->set_visible(false);
}
