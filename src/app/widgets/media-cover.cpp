#include "media-cover.hpp"
#include "spinner.hpp"
#include "../resources/thumbnails.hpp"

#include <imgui-ui/runtime.hpp>
#include <imgui-ui/surface.hpp>
#include <algorithm>

using namespace ui;

MediaCoverWidget::MediaCoverWidget(const MusicItem& item, ThumbnailCache& thumbnails, float size) : m_thumbnails(thumbnails) {
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
    m_sources = item.thumbnails;
    m_url.clear();
    m_download = {};
    m_pixel_size = 0;
    m_loaded = false;
    set_texture(nullptr);
    m_spinner->set_visible(!m_sources.empty());
}

void MediaCoverWidget::apply_theme_defaults(const Theme& theme) {
    ImageWidget::apply_theme_defaults(theme);

    configure_all_styles([&theme](Style& style) {
        style.color(rgba(255, 255, 255, 255)).border_radius(theme.controls.rounding);
    });
}

void MediaCoverWidget::draw_before() {
    const auto rect = layout().visual_rect();
    if (!ImGui::IsRectVisible(rect.min, rect.max)) {
        return;
    }

    m_last_draw_frame = ImGui::GetFrameCount();

    const auto scale = ImGui::GetIO().DisplayFramebufferScale;
    const float pixel_size = std::max(rect.size().x * scale.x, rect.size().y * scale.y);

    // only reselect when the display size changes, the source list is fixed until set_item
    if (pixel_size != m_pixel_size) {
        m_pixel_size = pixel_size;

        const auto* source = select_thumbnail(m_sources, pixel_size);
        const std::string_view url = source ? source->url : std::string_view{};
        if (url != m_url) {
            m_url = url;
            m_download.reset();
            m_loaded = false;
            set_texture(nullptr);
            m_spinner->set_visible(!m_url.empty());
        }
    }

    if (m_loaded) {
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

void MediaCoverWidget::on_update(float) {
    if ((m_loaded || m_download) && m_last_draw_frame < ImGui::GetFrameCount() - 1) {
        m_download.reset();
        set_texture(nullptr);
        m_loaded = false;
        m_spinner->set_visible(!m_url.empty());
    }
}
