#pragma once

#include "context-menu.hpp"
#include <arbor/layout/container.hpp>
#include <functional>
#include <memory>

struct CatalogPageState;
class MusiqText;
class ThumbnailCache;
class CatalogTilesWidget;
struct MusicItem;
struct MusicPage;

struct CatalogActions {
    std::function<void(MusicItem)> play;
    MediaMenu menu;
    std::function<void(MusicItem)> play_playlist;
};

class CatalogResultsWidget final : public ui::Container {
public:
    CatalogResultsWidget(
        std::string id, std::string title, ThumbnailCache& thumbnails, CatalogActions actions, bool home = false
    );
    void show(const CatalogPageState& state);
    bool near_bottom() const;

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void rebuild();

    ThumbnailCache& m_thumbnails;
    CatalogActions m_actions;
    MusiqText* m_status;
    ui::Container* m_rows;
    std::shared_ptr<const MusicPage> m_page;
    const bool m_home;
    std::size_t m_shelf_count = 0;
    CatalogTilesWidget* m_singleton_playlists = nullptr;
};
