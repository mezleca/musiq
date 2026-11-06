#pragma once

#include "../youtube/session.hpp"
#include "../youtube/schema/catalog.hpp"
#include "../youtube/error.hpp"
#include "../utils/debounce.hpp"

#include <chrono>
#include <deque>
#include <future>
#include <unordered_map>

struct CatalogPageState {
    std::string query;
    std::optional<MusicPage> page;
    std::string error;
    bool loading = false;
};

struct CatalogChanges {
    bool home = false;
    std::string search;
    bool suggestions = false;
};

class CatalogBrowser {
public:
    // keeps the search text while clearing cached pages, results from the previous session are ignored
    void set_session(const YoutubeMusicSession& session);
    bool home(bool refresh = false);
    bool search(std::string query, bool refresh = false);
    void suggest(std::string query);

    // returns which views need rebuilding after collecting completed requests and starting queued work
    CatalogChanges update(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    const CatalogPageState& home_state() const {
        return m_home;
    }

    const CatalogPageState& search_state() const {
        return search_state(m_query);
    }

    const CatalogPageState& search_state(const std::string& query) const;

    const MusicSuggestions& suggestions() const {
        return m_suggestions;
    }

private:
    void queue_page(std::string query);

    YoutubeMusicSession m_session;
    CatalogPageState m_home;
    std::unordered_map<std::string, CatalogPageState> m_searches;
    CatalogPageState m_empty_search;
    std::string m_query;
    MusicSuggestions m_suggestions;
    std::future<YoutubeResult<MusicPage>> m_page_task;
    std::future<YoutubeResult<MusicSuggestions>> m_suggestions_task;
    std::deque<std::string> m_pending_pages;
    std::string m_running_page;
    std::string m_suggestion_query;
    std::string m_running_suggestion;
    Debounce m_suggestions_debounce{std::chrono::milliseconds(300)};
    unsigned m_generation = 0;
    unsigned m_page_generation = 0;
    unsigned m_suggestions_generation = 0;
};
