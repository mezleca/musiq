#include "catalog.hpp"
#include "../widgets/catalog-results.hpp"

#include <utility>

using namespace ui;

CatalogOverlay::CatalogOverlay(std::string id, std::string title, ThumbnailCache& thumbnails)
    : Container(std::move(id), StackDirection::Vertical) {
    set_type_name("CatalogOverlay");
    set_size({grow(), grow()});
    m_results = &add<CatalogResultsWidget>(std::move(title), thumbnails);
}

void CatalogOverlay::set_title(std::string title) {
    m_results->set_title(std::move(title));
}
