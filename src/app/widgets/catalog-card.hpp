#pragma once

#include <imgui-ui/layout/container.hpp>

struct MusicItem;
class ThumbnailCache;

bool is_playlist_card(const MusicItem& item);

class MediaCardWidget final : public ui::Container {
public:
    MediaCardWidget(const MusicItem& item, ThumbnailCache& thumbnails);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
};

class PlaylistCardWidget final : public ui::Container {
public:
    PlaylistCardWidget(const MusicItem& item, ThumbnailCache& thumbnails);
};
