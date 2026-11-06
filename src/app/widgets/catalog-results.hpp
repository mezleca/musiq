#pragma once

#include <imgui-ui/layout/container.hpp>

struct CatalogPageState;
class MusiqText;
class ThumbnailCache;

class CatalogResultsWidget final : public ui::Container {
public:
    CatalogResultsWidget(std::string title, ThumbnailCache& thumbnails);
    void set_title(std::string title);
    void show(const CatalogPageState& state);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    ThumbnailCache& m_thumbnails;
    MusiqText* m_title;
    MusiqText* m_status;
    ui::Container* m_rows;
};
