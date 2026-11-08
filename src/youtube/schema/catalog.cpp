#include "catalog.hpp"

#include <stdexcept>
#include <utility>

void MusicPage::set_scope(MusicRoute route, const MusicRequestOptions& options, std::string_view identity) {
    if (continuation) continuation->set_scope(route, options, identity);
    for (auto& shelf : shelves) {
        if (shelf.continuation) shelf.continuation->set_scope(route, options, identity);
    }
}

void to_json(json& data, const MusicSearchRequest& value) {
    data = {{"query", value.query}};
    if (!value.params.empty()) data["params"] = value.params;
}

void to_json(json& data, const MusicBrowseRequest& value) {
    data = {{"browseId", value.browse_id}};
    if (!value.params.empty()) data["params"] = value.params;
}

static void append_catalog_item(const json& entry, std::vector<MusicItem>& items) {
    for (const auto key : {"musicResponsiveListItemRenderer", "musicTwoRowItemRenderer"}) {
        const auto item = entry.find(key);
        if (item != entry.end()) {
            items.push_back(item->get<MusicItem>());
            return;
        }
    }
}

static std::vector<MusicItem> catalog_items(const json& contents) {
    std::vector<MusicItem> items;
    items.reserve(contents.size());
    for (const auto& entry : contents) {
        append_catalog_item(entry, items);
    }
    return items;
}

static void append_shelf(const json& entry, std::vector<MusicShelf>& shelves) {
    const auto section = entry.find("itemSectionRenderer");
    if (section != entry.end()) {
        MusicShelf shelf;
        shelf.items = catalog_items(section->at("contents"));
        if (!shelf.items.empty()) shelves.push_back(std::move(shelf));
        return;
    }
    const auto card = entry.find("musicCardShelfRenderer");
    if (card != entry.end()) {
        MusicShelf shelf;
        shelf.items.push_back(card->get<MusicItem>());
        const auto contents = card->find("contents");
        if (contents != card->end()) {
            for (const auto& item : *contents) {
                append_catalog_item(item, shelf.items);
            }
        }
        shelves.push_back(std::move(shelf));
        return;
    }
    for (const auto key : {"musicShelfRenderer", "musicPlaylistShelfRenderer", "musicCarouselShelfRenderer", "gridRenderer"}) {
        const auto renderer = entry.find(key);
        if (renderer == entry.end()) continue;

        MusicShelf shelf;
        const auto title = renderer->find("title");
        const json::json_pointer carousel_title("/header/musicCarouselShelfBasicHeaderRenderer/title");
        if (title != renderer->end()) {
            shelf.title = music_text(*title);
        } else if (renderer->contains(carousel_title)) {
            shelf.title = music_text(renderer->at(carousel_title));
        }
        shelf.items = catalog_items(renderer->at(std::string_view(key) == "gridRenderer" ? "items" : "contents"));
        shelf.continuation = music_continuation(*renderer);
        shelves.push_back(std::move(shelf));
        return;
    }
}

void from_json(const json& data, MusicPage& value) {
    MusicPage page;
    bool found = false;
    static const auto paths = {
        json::json_pointer("/contents/tabbedSearchResultsRenderer/tabs/0/tabRenderer/content/sectionListRenderer"),
        json::json_pointer("/contents/singleColumnBrowseResultsRenderer/tabs/0/tabRenderer/content/sectionListRenderer"),
        json::json_pointer("/contents/twoColumnBrowseResultsRenderer/tabs/0/tabRenderer/content/sectionListRenderer"),
        json::json_pointer("/contents/twoColumnBrowseResultsRenderer/secondaryContents/sectionListRenderer"),
        json::json_pointer("/continuationContents/sectionListContinuation")
    };
    for (const auto& pointer : paths) {
        if (!data.contains(pointer)) continue;
        found = true;
        const auto& section = data.at(pointer);
        for (const auto& entry : section.at("contents")) {
            append_shelf(entry, page.shelves);
        }
        const auto continuation = music_continuation(section);
        if (continuation) page.continuation = continuation;
    }

    static const auto continuation_paths = {
        json::json_pointer("/continuationContents/musicShelfContinuation"),
        json::json_pointer("/continuationContents/musicPlaylistShelfContinuation"),
        json::json_pointer("/continuationContents/gridContinuation")
    };
    for (const auto& pointer : continuation_paths) {
        if (!data.contains(pointer)) continue;
        found = true;
        const auto& section = data.at(pointer);
        MusicShelf shelf;
        shelf.items = catalog_items(section.at(pointer.back() == "gridContinuation" ? "items" : "contents"));
        shelf.continuation = music_continuation(section);
        page.continuation = shelf.continuation;
        page.shelves.push_back(std::move(shelf));
    }

    if (!found) throw std::invalid_argument("unsupported catalog page layout");
    value = std::move(page);
}

void to_json(json& data, const MusicPage& value) {
    data = {{"shelves", json::array()}};
    for (const auto& shelf : value.shelves) {
        json section = {{"title", shelf.title}, {"items", shelf.items}};
        if (shelf.continuation) section["continuation"] = *shelf.continuation;
        data["shelves"].push_back(std::move(section));
    }
    if (value.continuation) data["continuation"] = *value.continuation;
}

void from_json(const json& data, MusicSuggestions& value) {
    MusicSuggestions suggestions;
    for (const auto& entry : data.at("contents")) {
        const auto section = entry.find("searchSuggestionsSectionRenderer");
        if (section == entry.end()) continue;
        for (const auto& item : section->at("contents")) {
            const auto history = item.find("historySuggestionRenderer");
            const auto query = item.find("searchSuggestionRenderer");
            if (history != item.end() || query != item.end()) {
                const auto& suggestion = history != item.end() ? *history : *query;
                const auto query_text = suggestion.at("navigationEndpoint").at("searchEndpoint").at("query").get<std::string>();
                suggestions.queries.push_back({query_text, history != item.end()});
            }
        }
    }
    value = std::move(suggestions);
}

void to_json(json& data, const MusicQuerySuggestion& value) {
    data = {{"query", value.query}, {"history", value.history}};
}

void to_json(json& data, const MusicSuggestions& value) {
    data = {{"queries", value.queries}};
}
