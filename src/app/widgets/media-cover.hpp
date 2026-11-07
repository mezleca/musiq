#pragma once

#include <imgui-ui/widgets/image.hpp>
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
    void on_update(float dt) override;
    void draw_before() override;

private:
    ThumbnailCache& m_thumbnails;
    SpinnerWidget* m_spinner;
    std::vector<MusicThumbnail> m_sources;
    std::string m_url;
    std::shared_ptr<ThumbnailRequest> m_download;
    float m_pixel_size = 0;
    int m_last_draw_frame = -1;
    bool m_loaded = false;
};
