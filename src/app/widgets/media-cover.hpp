#pragma once

#include <arbor/widgets/image.hpp>
#include "../../youtube/schema/common.hpp"
#include <memory>
#include <span>

class ThumbnailCache;
struct ThumbnailRequest;

class MediaCoverWidget : public ui::ImageWidget {
public:
    MediaCoverWidget(
        ThumbnailCache& thumbnails, float size, std::span<const MusicThumbnail> sources = {}, bool preserve_aspect = false
    );
    void set_thumbnails(std::span<const MusicThumbnail> thumbnails);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
    void draw_before() override;
    void paint_content(const ui::PaintContext& context) override;

private:
    ThumbnailCache& m_thumbnails;
    std::string m_url;
    std::shared_ptr<ThumbnailRequest> m_download;
    const float m_size;
    const bool m_preserve_aspect;
    bool m_loaded = false;
};
