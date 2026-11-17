#pragma once

#include "session.hpp"
#include "schema/catalog.hpp"
#include "error.hpp"
#include "../utils/debounce.hpp"
#include "../utils/task-queue.hpp"

#include <chrono>
#include <deque>
#include <unordered_map>
#include <memory>
#include <string_view>
#include <cstdint>

struct CatalogPageState {
    std::string query;
    std::shared_ptr<MusicPage> page;
    std::string error;
    bool loading = false;
};

struct CatalogChanges {
    bool home = false;
    std::string search;
    bool suggestions = false;
    bool library = false;
};

class CatalogBrowser {
public:
    CatalogBrowser();
    void set_session(const YoutubeMusicSession& session);
    const std::vector<MusicItem>& library_playlists() const {
        return m_library;
    }
    bool home(bool refresh = false);
    bool more_home();
    bool search(std::string query, bool refresh = false);
    void suggest(std::string query);

    // reloads the saved playlists for the current session, keeping home and search state
    void refresh_library();
    void remove_playlist(std::string_view browse_id);

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
    void prune_searches();
    void
    finish_library(std::uint64_t generation, std::uint64_t request, TaskOutcome<YoutubeResult<std::vector<MusicItem>>> outcome);
    void finish_home_more(std::uint64_t generation, TaskOutcome<YoutubeResult<MusicPage>> outcome);
    void finish_page(std::uint64_t generation, std::string query, TaskOutcome<YoutubeResult<MusicPage>> outcome);
    void finish_suggestions(std::uint64_t generation, std::string query, TaskOutcome<YoutubeResult<MusicSuggestions>> outcome);

    YoutubeMusicSession m_session;
    CatalogPageState m_home;
    std::unordered_map<std::string, CatalogPageState> m_searches;
    std::deque<std::string> m_search_order;
    std::string m_query;
    MusicSuggestions m_suggestions;
    std::vector<MusicItem> m_library;
    TaskQueue& m_tasks = TaskQueue::instance();
    std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);
    CatalogChanges m_changes;
    std::deque<std::string> m_pages;
    std::string m_suggestion_query;
    std::uint64_t m_generation = 0;
    std::uint64_t m_library_request = 0;
    bool m_page_loading = false;
    bool m_suggestions_loading = false;
    Debounce m_suggestions_debounce{std::chrono::milliseconds(300)};
};
