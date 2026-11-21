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
#include <unordered_set>

enum class CatalogPageStatus {
    Idle,
    Loading,
    Ready,
    Failed
};

struct CatalogPageState {
    bool loading() const {
        return status == CatalogPageStatus::Loading;
    }

    std::string query;
    std::shared_ptr<MusicPage> page;
    std::string error;
    CatalogPageStatus status = CatalogPageStatus::Idle;
};

struct CatalogChanges {
    bool home = false;
    std::vector<std::string> searches;
    bool suggestions = false;
    bool library = false;
};

class CatalogBrowser {
public:
    CatalogBrowser() = default;
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
    void add_created_playlist(MusicItem item);
    void update_playlist_cover(std::string_view browse_id, const std::vector<MusicThumbnail>& thumbnails);
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
    void load_library_page(std::optional<MusicContinuation> continuation = {});
    void accept_library(YoutubeResult<MusicPage> result);
    void accept_page(std::string query, bool append, YoutubeResult<MusicPage> result);

    YoutubeMusicSession m_session;
    CatalogPageState m_home;
    std::unordered_map<std::string, CatalogPageState> m_searches;
    std::deque<std::string> m_search_order;
    std::string m_query;
    MusicSuggestions m_suggestions;
    std::vector<MusicItem> m_library;
    std::vector<MusicItem> m_pending_library;
    std::unordered_set<std::string> m_created_playlist_ids;
    std::unordered_set<std::string> m_library_ids;
    std::unordered_set<std::string> m_library_tokens;
    TaskQueue& m_tasks = TaskQueue::instance();
    TaskScope m_pages;
    TaskScope m_library_tasks;
    TaskScope m_suggestion_tasks;
    CatalogChanges m_changes;
    std::string m_suggestion_query;
    Debounce m_suggestions_debounce{std::chrono::milliseconds(300)};
};
