#pragma once

#include <arbor/layout/container.hpp>
#include <functional>
#include <memory>

struct CatalogPageState;
class MusiqText;
class ThumbnailCache;
struct MusicItem;
struct MusicPage;

struct CatalogActions {
    std::function<void(MusicItem)> play;
    std::function<void(MusicItem)> play_next;
    std::function<void(MusicItem)> append;
};

class CatalogResultsWidget final : public ui::Container {
public:
    CatalogResultsWidget(std::string id, std::string title, ThumbnailCache& thumbnails);
    void show(const CatalogPageState& state, bool append = false);
    bool near_bottom() const;
    void set_actions(CatalogActions actions);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void rebuild(std::size_t first_shelf = 0);

    ThumbnailCache& m_thumbnails;
    CatalogActions m_actions;
    MusiqText* m_title;
    MusiqText* m_status;
    ui::Container* m_rows;
    std::shared_ptr<const MusicPage> m_page;
};
