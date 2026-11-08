#pragma once

#include <imgui-ui/layout/container.hpp>
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
    void show(const CatalogPageState& state);
    void set_actions(CatalogActions actions);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void rebuild();

    ThumbnailCache& m_thumbnails;
    CatalogActions m_actions;
    MusiqText* m_title;
    MusiqText* m_status;
    ui::Container* m_rows;
    std::shared_ptr<const MusicPage> m_page;
};
