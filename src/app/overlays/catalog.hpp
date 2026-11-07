#pragma once

#include <imgui-ui/layout/container.hpp>

#include <string>

class CatalogResultsWidget;
class ThumbnailCache;

class CatalogOverlay final : public ui::Container {
public:
    CatalogOverlay(std::string id, std::string title, ThumbnailCache& thumbnails);
    CatalogResultsWidget& results() {
        return *m_results;
    }

    void set_title(std::string title);

private:
    CatalogResultsWidget* m_results;
};
