#pragma once

#include <imgui-ui/layout/container.hpp>

class CatalogResultsWidget;
class ThumbnailCache;

class HomeOverlay final : public ui::Container {
public:
    explicit HomeOverlay(ThumbnailCache& thumbnails);

    CatalogResultsWidget& results() {
        return *m_results;
    }

private:
    CatalogResultsWidget* m_results;
};
