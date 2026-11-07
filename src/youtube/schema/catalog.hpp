#pragma once

#include "common.hpp"
#include "continuation.hpp"

struct MusicShelf {
    std::string title;
    std::vector<MusicItem> items;
    std::optional<MusicContinuation> continuation;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/browsing.html#ytmusicapi.YTMusic.get_home
struct MusicPage {
    void set_scope(MusicRoute route, const MusicRequestOptions& options, std::string_view identity);

    std::vector<MusicShelf> shelves;
    std::optional<MusicContinuation> continuation;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/search.html#ytmusicapi.YTMusic.search
struct MusicSearchRequest {
    std::string query;
    std::string params;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/browsing.html#ytmusicapi.YTMusic.get_home
struct MusicBrowseRequest {
    std::string browse_id;
    std::string params;
};

struct MusicQuerySuggestion {
    std::string query;
    bool history = false;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/search.html#ytmusicapi.YTMusic.get_search_suggestions
struct MusicSuggestions {
    std::vector<MusicQuerySuggestion> queries;
};

void to_json(json& data, const MusicSearchRequest& value);
void to_json(json& data, const MusicBrowseRequest& value);
void from_json(const json& data, MusicPage& value);
void to_json(json& data, const MusicPage& value);
void from_json(const json& data, MusicSuggestions& value);
void to_json(json& data, const MusicSuggestions& value);
void to_json(json& data, const MusicQuerySuggestion& value);
