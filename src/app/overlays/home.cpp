#include "home.hpp"
#include "../widgets/catalog-results.hpp"

HomeOverlay::HomeOverlay(ThumbnailCache& thumbnails) : ui::Container("home", ui::StackDirection::Vertical) {
    set_type_name("HomeOverlay");
    set_size({ui::grow(), ui::grow()});
    m_results = &add<CatalogResultsWidget>("Home", thumbnails);
}
