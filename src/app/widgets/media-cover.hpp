#pragma once

#include <arbor/widgets/image.hpp>
#include "../../youtube/schema/common.hpp"
#include <memory>

class ThumbnailCache;
struct ThumbnailRequest;
class SpinnerWidget;

class MediaCoverWidget final : public ui::ImageWidget {
public:
    MediaCoverWidget(const MusicItem& item, ThumbnailCache& thumbnails, float size);
    void set_item(const MusicItem& item);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
    void draw_before() override;

private:
    ThumbnailCache& m_thumbnails;
    SpinnerWidget* m_spinner;
    std::string m_url;
    std::shared_ptr<ThumbnailRequest> m_download;
    const float m_size;
    bool m_loaded = false;
};
