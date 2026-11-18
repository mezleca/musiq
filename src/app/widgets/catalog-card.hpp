#pragma once

#include "button.hpp"
#include <cstddef>

struct MusicItem;
class ThumbnailCache;

class MediaCardWidget final : public MusiqButtonWidget {
public:
    MediaCardWidget(const MusicItem& item, ThumbnailCache& thumbnails, bool loading = false, std::size_t number = 0);
    void set_selected(bool selected);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    bool m_selected = false;
};

class PlaylistCardWidget final : public MusiqButtonWidget {
public:
    PlaylistCardWidget(const MusicItem& item, ThumbnailCache& thumbnails, bool loading = false);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
};
