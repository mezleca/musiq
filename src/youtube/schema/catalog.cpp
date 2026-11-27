#include "catalog.hpp"
#include "../../utils/media.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <type_traits>

void MusicPage::append(MusicPage page) {
    static_assert(std::is_nothrow_move_constructible_v<MusicShelf>);
    if (continuation && page.continuation && continuation->token == page.continuation->token) {
        page.continuation.reset();
    }

    // moving a shelf preserves its item buffer, including pointers held by existing card callbacks
    for (auto& shelf : page.shelves)
        shelves.push_back(std::move(shelf));
    continuation = std::move(page.continuation);
}

void MusicPage::set_scope(MusicRoute route, const MusicRequestOptions& options, std::string_view identity) {
    if (continuation) continuation->set_scope(route, options, identity);
    for (auto& shelf : shelves) {
        if (shelf.continuation) shelf.continuation->set_scope(route, options, identity);
    }
}

void MusicPage::group_search_results() {
    const auto flat = [](const MusicShelf& shelf) { return shelf.title.empty() && !shelf.carousel && !shelf.continuation; };
    const auto first = std::ranges::find_if(shelves, flat);
    if (first == shelves.end()) return;

    MusicShelf songs{.title = "Songs"};
    MusicShelf videos{.title = "Videos"};
    MusicShelf albums{.title = "Albums"};
    MusicShelf artists{.title = "Artists"};
    MusicShelf playlists{.title = "Playlists"};
    MusicShelf other{.title = "Other results"};
    for (auto shelf = first; shelf != shelves.end(); ++shelf) {
        if (!flat(*shelf)) continue;
        for (auto& item : shelf->items) {
            const auto& endpoint = item.endpoint;
            auto* group = &other;
            if (is_album(endpoint))
                group = &albums;
            else if (utils::media::is_playlist_card(item))
                group = &playlists;
            else if (!endpoint.video_id.empty())
                group = endpoint.music_video ? &videos : &songs;
            else if (endpoint.browse_id.starts_with("UC") || endpoint.page_type == "MUSIC_PAGE_TYPE_ARTIST")
                group = &artists;
            group->items.push_back(std::move(item));
        }
    }

    const auto index = first - shelves.begin();
    std::erase_if(shelves, flat);
    auto position = shelves.begin() + index;
    for (auto* group : {&songs, &videos, &albums, &artists, &playlists, &other}) {
        if (!group->items.empty()) position = shelves.insert(position, std::move(*group)) + 1;
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
        const json::json_pointer title("/header/musicCardShelfHeaderBasicRenderer/title");
        shelf.title = card->contains(title) ? music_text(card->at(title)) : "Top result";
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
        shelf.carousel = std::string_view(key) == "musicCarouselShelfRenderer";
        const auto title = renderer->find("title");
        const json::json_pointer carousel_title("/header/musicCarouselShelfBasicHeaderRenderer/title");
        if (title != renderer->end()) {
            shelf.title = music_text(*title);
        } else if (renderer->contains(carousel_title)) {
            shelf.title = music_text(renderer->at(carousel_title));
        }
        const auto& items = renderer->at(std::string_view(key) == "gridRenderer" ? "items" : "contents");
        shelf.items = catalog_items(items);
        shelf.continuation = music_continuation(*renderer);
        if (!shelf.continuation && !items.empty()) shelf.continuation = music_continuation_item(items.back());
        shelves.push_back(std::move(shelf));
        return;
    }
}

void from_json(const json& data, MusicPage& value) {
    MusicPage page;
    bool found = false;
    const auto append_sections = [&page](const json& entries) {
        for (const auto& entry : entries) {
            if (auto continuation = music_continuation_item(entry)) {
                page.continuation = std::move(continuation);
            } else {
                append_shelf(entry, page.shelves);
            }
        }
    };
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
        append_sections(section.at("contents"));
        auto continuation = music_continuation(section);
        if (continuation && !page.continuation) page.continuation = std::move(continuation);
    }

    for (const auto key : {"onResponseReceivedActions", "onResponseReceivedCommands", "onResponseReceivedEndpoints"}) {
        const auto actions = data.find(key);
        if (actions == data.end()) continue;
        const json::json_pointer items("/appendContinuationItemsAction/continuationItems");
        for (const auto& action : *actions) {
            if (!action.contains(items)) continue;
            found = true;
            const auto& entries = action.at(items);
            append_sections(entries);
            auto tracks = catalog_items(entries);
            if (!tracks.empty()) page.shelves.push_back({.items = std::move(tracks)});
        }
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
        const auto& items = section.at(pointer.back() == "gridContinuation" ? "items" : "contents");
        shelf.items = catalog_items(items);
        shelf.continuation = music_continuation(section);
        if (!shelf.continuation && !items.empty()) shelf.continuation = music_continuation_item(items.back());
        page.continuation = shelf.continuation;
        page.shelves.push_back(std::move(shelf));
    }

    if (!found) throw std::invalid_argument("unsupported catalog page layout");
    value = std::move(page);
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
                auto query_text = suggestion.at("navigationEndpoint").at("searchEndpoint").at("query").get<std::string>();
                suggestions.queries.push_back({std::move(query_text), history != item.end()});
            }
        }
    }
    value = std::move(suggestions);
}
