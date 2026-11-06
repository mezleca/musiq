#pragma once

#include <imgui-ui/layout/container.hpp>
#include <string_view>

class CatalogResultsWidget;
class ThumbnailCache;

class SearchOverlay final : public ui::Container {
public:
    explicit SearchOverlay(ThumbnailCache& thumbnails);
    void set_query(std::string_view query);

    CatalogResultsWidget& results() {
        return *m_results;
    }

private:
    CatalogResultsWidget* m_results;
};
