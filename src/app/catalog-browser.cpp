#include "catalog-browser.hpp"
#include "../youtube/catalog.hpp"
#include "../youtube/client.hpp"
#include "../utils/log.hpp"

#include <exception>
#include <format>
#include <utility>

static YoutubeResult<MusicPage> read_page(const YoutubeMusicSession& session, const std::string& query) {
    const YoutubeMusicClient client(session);
    const YoutubeMusicCatalog catalog(client);
    return query.empty() ? catalog.browse({.browse_id = "FEmusic_home"}) : catalog.search({.query = query});
}

static YoutubeResult<MusicSuggestions> read_suggestions(const YoutubeMusicSession& session, const std::string& query) {
    const YoutubeMusicClient client(session);
    return YoutubeMusicCatalog(client).suggestions(query, {.auth = MusicAuth::Anonymous});
}

template <typename T>
static bool ready(const std::future<T>& task) {
    return task.valid() && task.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

template <typename T>
static YoutubeResult<T> take_result(std::future<YoutubeResult<T>>& task) {
    try {
        return task.get();
    } catch (const std::exception& error) {
        return youtube_error(YoutubeErrorKind::Transport, error.what());
    }
}

void CatalogBrowser::set_session(const YoutubeMusicSession& session) {
    m_session = session;
    ++m_generation;
    m_home = {};
    m_searches.clear();

    m_pending_pages.clear();
    suggest({});
}

const CatalogPageState& CatalogBrowser::search_state(const std::string& query) const {
    const auto entry = m_searches.find(query);
    return entry == m_searches.end() ? m_empty_search : entry->second;
}

void CatalogBrowser::queue_page(std::string query) {
    auto& state = query.empty() ? m_home : m_searches.at(query);
    state.loading = true;
    state.error.clear();
    m_pending_pages.push_back(std::move(query));
}

bool CatalogBrowser::home(bool refresh) {
    if (m_home.loading || (!refresh && m_home.page)) return false;

    queue_page({});
    return true;
}

bool CatalogBrowser::search(std::string query, bool refresh) {
    m_query = std::move(query);
    if (m_query.empty()) return false;

    auto& state = m_searches[m_query];
    if (state.loading || (!refresh && state.page)) return false;

    state.query = m_query;
    state.page.reset();
    queue_page(m_query);
    suggest({});
    return true;
}

void CatalogBrowser::suggest(std::string query) {
    m_suggestion_query = std::move(query);
    m_suggestions = {};

    if (m_suggestion_query.empty()) {
        m_suggestions_debounce.cancel();
    } else {
        m_suggestions_debounce.restart();
    }
}

CatalogChanges CatalogBrowser::update(std::chrono::steady_clock::time_point now) {
    CatalogChanges changed;

    if (ready(m_page_task)) {
        auto result = take_result(m_page_task);
        const bool home = m_running_page.empty();

        // completed searches stay cached even when another page is visible, but never cross sessions
        if (m_page_generation == m_generation) {
            auto& state = home ? m_home : m_searches.at(m_running_page);
            state.loading = false;
            if (result) {
                state.page = std::move(*result);
                LOG_INFO("[catalog] {} loaded (shelves={})", home ? "home" : "search", state.page->shelves.size());
            } else {
                state.error = result.error().message;
                if (result.error().status != 0) state.error += std::format(" (HTTP {})", result.error().status);
                LOG_ERROR(
                    "[catalog] {} failed: {} (status={}, transport={})", home ? "home" : "search", result.error().message,
                    result.error().status, result.error().transport_code
                );
            }

            changed.home = home;
            if (!home) changed.search = m_running_page;
        }
    }

    if (ready(m_suggestions_task)) {
        auto result = take_result(m_suggestions_task);
        if (m_suggestions_generation == m_generation && m_running_suggestion == m_suggestion_query &&
            !m_suggestion_query.empty()) {
            if (result) {
                m_suggestions = std::move(*result);
            } else {
                LOG_ERROR("[catalog] suggestions failed: {} (status={})", result.error().message, result.error().status);
            }
            changed.suggestions = true;
        }
    }

    // replacing an unfinished async future would block the ui until its worker exits
    if (!m_pending_pages.empty() && !m_page_task.valid()) {
        m_running_page = std::move(m_pending_pages.front());
        m_pending_pages.pop_front();
        m_page_generation = m_generation;
        LOG_INFO("[catalog] loading {}", m_running_page.empty() ? "home" : "search");
        m_page_task =
            std::async(std::launch::async, [session = m_session, query = m_running_page] { return read_page(session, query); });
    }

    if (!m_suggestions_task.valid() && m_suggestions_debounce.poll(now)) {
        m_running_suggestion = m_suggestion_query;
        m_suggestions_generation = m_generation;
        m_suggestions_task = std::async(std::launch::async, [session = m_session, query = m_running_suggestion] {
            return read_suggestions(session, query);
        });
    }

    return changed;
}
