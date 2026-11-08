#pragma once

#include "session.hpp"
#include "schema/catalog.hpp"
#include "error.hpp"
#include "../utils/debounce.hpp"

#include <chrono>
#include <deque>
#include <future>
#include <unordered_map>
#include <memory>

class ThreadPool;

struct CatalogPageState {
    std::string query;
    std::shared_ptr<const MusicPage> page;
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
    explicit CatalogBrowser(ThreadPool& workers);
    // drops cached pages and pending futures so responses from the previous session cannot be installed
    void set_session(const YoutubeMusicSession& session);
    bool home(bool refresh = false);
    bool search(std::string query, bool refresh = false);
    void suggest(std::string query);

    // collects a finished page or suggestion task and starts the next queued request
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

    std::shared_ptr<const YoutubeMusicSession> m_session;
    ThreadPool& m_workers;
    CatalogPageState m_home;
    std::unordered_map<std::string, CatalogPageState> m_searches;
    std::string m_query;
    MusicSuggestions m_suggestions;
    std::future<YoutubeResult<MusicPage>> m_page_task;
    std::future<YoutubeResult<MusicSuggestions>> m_suggestions_task;
    std::deque<std::string> m_pages;
    std::string m_suggestion_query;
    std::string m_running_suggestion;
    Debounce m_suggestions_debounce{std::chrono::milliseconds(300)};
};
