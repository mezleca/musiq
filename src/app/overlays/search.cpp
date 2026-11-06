#include "search.hpp"
#include "../widgets/catalog-results.hpp"

#include <string>

SearchOverlay::SearchOverlay(ThumbnailCache& thumbnails) : ui::Container("search", ui::StackDirection::Vertical) {
    set_type_name("SearchOverlay");
    set_size({ui::grow(), ui::grow()});
    set_visible(false);
    m_results = &add<CatalogResultsWidget>("Search", thumbnails);
}

void SearchOverlay::set_query(std::string_view query) {
    m_results->set_title(query.empty() ? "Search" : "Results for: " + std::string(query));
}
