#include "catalog-browser.hpp"
#include "catalog.hpp"
#include "client.hpp"
#include "../utils/future.hpp"
#include "../utils/log.hpp"
#include "../utils/thread-pool.hpp"

#include <exception>
#include <format>
#include <utility>

static YoutubeResult<MusicPage> read_page(const YoutubeMusicSession& session, const std::string& query) {
    const YoutubeMusicClient client(session);
    const YoutubeMusicCatalog catalog(client);
    return query.empty() ? catalog.browse({.browse_id = "FEmusic_home", .params = {}})
                         : catalog.search({.query = query, .params = {}});
}

static YoutubeResult<MusicSuggestions> read_suggestions(const YoutubeMusicSession& session, const std::string& query) {
    const YoutubeMusicClient client(session);
    return YoutubeMusicCatalog(client).suggestions(query, {.auth = MusicAuth::Anonymous});
}

template <typename T>
static YoutubeResult<T> take_result(std::future<YoutubeResult<T>>& task) {
    try {
        return task.get();
    } catch (const std::exception& error) {
        return youtube_error(YoutubeErrorKind::Transport, error.what());
    }
}

CatalogBrowser::CatalogBrowser(ThreadPool& workers)
    : m_session(std::make_shared<const YoutubeMusicSession>()), m_workers(workers) {}

void CatalogBrowser::set_session(const YoutubeMusicSession& session) {
    m_session = std::make_shared<const YoutubeMusicSession>(session);
    m_home = {};
    m_searches.clear();

    // dropping the futures ignores old responses, queued requests still finish in the pool
    m_page_task = {};
    m_suggestions_task = {};
    m_pages.clear();
    suggest({});
}

const CatalogPageState& CatalogBrowser::search_state(const std::string& query) const {
    static const CatalogPageState empty;
    const auto entry = m_searches.find(query);
    return entry == m_searches.end() ? empty : entry->second;
}

void CatalogBrowser::queue_page(std::string query) {
    auto& state = query.empty() ? m_home : m_searches.at(query);
    state.loading = true;
    state.error.clear();
    m_pages.push_back(std::move(query));
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

    // keep the active request at the front until its result has updated the original page
    if (future_ready(m_page_task)) {
        auto result = take_result(m_page_task);
        auto query = std::move(m_pages.front());
        m_pages.pop_front();
        const bool home = query.empty();

        auto& state = home ? m_home : m_searches.at(query);
        state.loading = false;
        if (result) {
            state.page = std::make_shared<MusicPage>(std::move(*result));
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
        if (!home) changed.search = std::move(query);
    }

    if (future_ready(m_suggestions_task)) {
        auto result = take_result(m_suggestions_task);
        // typing can overtake a worker, so stale suggestions must not replace the current query
        if (m_running_suggestion == m_suggestion_query && !m_suggestion_query.empty()) {
            if (result) {
                m_suggestions = std::move(*result);
            } else {
                LOG_ERROR("[catalog] suggestions failed: {} (status={})", result.error().message, result.error().status);
            }
            changed.suggestions = true;
        }
    }

    if (!m_pages.empty() && !m_page_task.valid()) {
        LOG_INFO("[catalog] loading {}", m_pages.front().empty() ? "home" : "search");
        m_page_task = m_workers.enqueue([session = m_session, query = m_pages.front()] { return read_page(*session, query); });
    }

    if (!m_suggestions_task.valid() && m_suggestions_debounce.poll(now)) {
        m_running_suggestion = m_suggestion_query;
        m_suggestions_task =
            m_workers.enqueue([session = m_session, query = m_running_suggestion] { return read_suggestions(*session, query); });
    }

    return changed;
}
