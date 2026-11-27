#pragma once

#include "button.hpp"
#include "context-menu.hpp"
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
    const bool m_numbered;
    bool m_selected = false;
};

class CatalogTileWidget final : public MusiqButtonWidget {
public:
    explicit CatalogTileWidget(
        const MusicItem& item, ThumbnailCache& thumbnails, OpenMenu menu = {}, std::function<void()> play = {}
    );

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
};
