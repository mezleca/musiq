#pragma once

#include "button.hpp"

struct MusicItem;
class ThumbnailCache;

bool is_playlist_card(const MusicItem& item);
std::string media_artist(const MusicItem& item);

class MediaCardWidget final : public MusiqButtonWidget {
public:
    MediaCardWidget(const MusicItem& item, ThumbnailCache& thumbnails);
    void set_selected(bool selected);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    bool m_selected = false;
};

class PlaylistCardWidget final : public MusiqButtonWidget {
public:
    PlaylistCardWidget(const MusicItem& item, ThumbnailCache& thumbnails);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
};
