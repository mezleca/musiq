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
    m_search.page.reset();
    m_search.error.clear();
    m_search.loading = false;

    m_pending_page.reset();
    suggest({});
}

void CatalogBrowser::home(bool refresh) {
    if (!refresh && m_home.page) return;

    m_home.loading = true;
    m_home.error.clear();
    m_pending_page = std::string{};
}

void CatalogBrowser::search(std::string query, bool refresh) {
    if (query.empty() || (!refresh && query == m_search.query && m_search.page)) return;

    m_search.query = std::move(query);
    m_search.page.reset();
    m_search.error.clear();
    m_search.loading = true;
    m_pending_page = m_search.query;
    suggest({});
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

        // a login change or newer search can make a completed request obsolete
        if (m_page_generation == m_generation && (home || m_running_page == m_search.query)) {
            auto& state = home ? m_home : m_search;
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
            changed.search = !home;
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
    if (m_pending_page && !m_page_task.valid()) {
        m_running_page = std::move(*m_pending_page);
        m_pending_page.reset();
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
