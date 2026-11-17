#include "catalog-browser.hpp"
#include "catalog.hpp"
#include "client.hpp"
#include "playlists.hpp"
#include "../utils/log.hpp"

#include <format>
#include <algorithm>
#include <exception>
#include <utility>

CatalogBrowser::CatalogBrowser() = default;

static std::string task_error_message(std::exception_ptr error) {
    try {
        std::rethrow_exception(error);
    } catch (const std::exception& exception) {
        return exception.what();
    } catch (...) {
        return "unknown task exception";
    }
}

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

void CatalogBrowser::set_session(const YoutubeMusicSession& session) {
    // old requests may finish after the session changes, so their callbacks must be ignored
    ++m_generation;
    ++m_library_request;
    m_page_loading = false;
    m_suggestions_loading = false;
    m_changes = {};
    m_session = session;
    m_home = {};
    m_searches.clear();
    m_search_order.clear();
    m_library.clear();
    refresh_library();

    m_pages.clear();
    suggest({});
}

void CatalogBrowser::refresh_library() {
    if (!m_session.user()) {
        m_library.clear();
        m_changes.library = true;
        return;
    }

    const auto generation = m_generation;
    const auto request = ++m_library_request;
    const std::weak_ptr<int> lifetime = m_lifetime;
    m_tasks.enqueue(
        [session = m_session] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).library();
        },
        [this, generation, request, lifetime](TaskOutcome<YoutubeResult<std::vector<MusicItem>>> outcome) {
            if (lifetime.expired()) {
                return;
            }
            finish_library(generation, request, std::move(outcome));
        }
    );
}

void CatalogBrowser::remove_playlist(std::string_view browse_id) {
    const auto playlist_id = browse_id.starts_with("VL") ? browse_id.substr(2) : browse_id;
    std::erase_if(m_library, [playlist_id](const MusicItem& item) {
        const std::string_view id = item.endpoint.browse_id;
        return id == playlist_id || (id.starts_with("VL") && id.substr(2) == playlist_id);
    });
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

bool CatalogBrowser::more_home() {
    if (m_home.loading || !m_home.error.empty() || !m_home.page || !m_home.page->continuation) return false;

    m_home.loading = true;
    const auto generation = m_generation;
    const std::weak_ptr<int> lifetime = m_lifetime;
    m_tasks.enqueue(
        [session = m_session, continuation = *m_home.page->continuation] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicCatalog(client).more(continuation);
        },
        [this, generation, lifetime](TaskOutcome<YoutubeResult<MusicPage>> outcome) {
            if (lifetime.expired()) {
                return;
            }
            finish_home_more(generation, std::move(outcome));
        }
    );
    return true;
}

bool CatalogBrowser::search(std::string query, bool refresh) {
    m_query = std::move(query);
    if (m_query.empty()) return false;

    std::erase(m_search_order, m_query);
    m_search_order.push_back(m_query);

    auto& state = m_searches[m_query];
    if (state.loading || (!refresh && state.page)) return false;

    state.query = m_query;
    state.page.reset();
    queue_page(m_query);
    suggest({});
    return true;
}

void CatalogBrowser::prune_searches() {
    for (auto entry = m_search_order.begin(); m_searches.size() > 12 && entry != m_search_order.end();) {
        const auto state = m_searches.find(*entry);
        if (*entry != m_query && !state->second.loading) {
            m_searches.erase(state);
            entry = m_search_order.erase(entry);
        } else {
            ++entry;
        }
    }
}

void CatalogBrowser::finish_library(
    std::uint64_t generation, std::uint64_t request, TaskOutcome<YoutubeResult<std::vector<MusicItem>>> outcome
) {
    if (generation != m_generation || request != m_library_request) {
        return;
    }

    m_changes.library = true;
    if (!outcome) {
        LOG_WARN("[catalog] library playlists task failed: {}", task_error_message(outcome.error()));
        return;
    }

    auto result = std::move(*outcome);
    if (result) {
        m_library = std::move(*result);
    } else {
        LOG_WARN("[catalog] library playlists failed: {}", result.error().message);
    }
}

void CatalogBrowser::finish_home_more(std::uint64_t generation, TaskOutcome<YoutubeResult<MusicPage>> outcome) {
    if (generation != m_generation) {
        return;
    }

    m_home.loading = false;
    m_changes.home = true;
    if (!outcome) {
        m_home.error = task_error_message(outcome.error());
        LOG_ERROR("[catalog] home continuation failed: {}", m_home.error);
        return;
    }

    auto result = std::move(*outcome);
    if (result) {
        m_home.page->append(std::move(*result));
    } else {
        m_home.error = result.error().message;
        LOG_ERROR("[catalog] home continuation failed: {}", m_home.error);
    }
}

void CatalogBrowser::finish_page(std::uint64_t generation, std::string query, TaskOutcome<YoutubeResult<MusicPage>> outcome) {
    if (generation != m_generation) {
        return;
    }

    m_page_loading = false;
    m_pages.pop_front();
    const bool home = query.empty();
    auto& state = home ? m_home : m_searches.at(query);
    state.loading = false;

    if (!outcome) {
        state.error = task_error_message(outcome.error());
        LOG_ERROR("[catalog] {} task failed: {}", home ? "home" : "search", state.error);
    } else {
        auto result = std::move(*outcome);
        if (result) {
            state.page = std::make_shared<MusicPage>(std::move(*result));
            LOG_INFO("[catalog] {} loaded (shelves={})", home ? "home" : "search", state.page->shelves.size());
        } else {
            state.error = result.error().message;
            if (result.error().status != 0) {
                state.error += std::format(" (HTTP {})", result.error().status);
            }
            LOG_ERROR(
                "[catalog] {} failed: {} (status={}, transport={})", home ? "home" : "search", result.error().message,
                result.error().status, result.error().transport_code
            );
        }
    }

    m_changes.home |= home;
    if (!home) {
        m_changes.search = std::move(query);
    }
}

void CatalogBrowser::finish_suggestions(
    std::uint64_t generation, std::string query, TaskOutcome<YoutubeResult<MusicSuggestions>> outcome
) {
    if (generation != m_generation) {
        return;
    }

    m_suggestions_loading = false;
    // typing can change the query while the request is running
    if (query != m_suggestion_query || query.empty()) {
        return;
    }

    if (!outcome) {
        LOG_ERROR("[catalog] suggestions task failed: {}", task_error_message(outcome.error()));
    } else {
        auto result = std::move(*outcome);
        if (result) {
            m_suggestions = std::move(*result);
        } else {
            LOG_ERROR("[catalog] suggestions failed: {} (status={})", result.error().message, result.error().status);
        }
    }

    m_changes.suggestions = true;
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
    auto changed = std::exchange(m_changes, {});

    if (!m_pages.empty() && !m_page_loading) {
        LOG_INFO("[catalog] loading {}", m_pages.front().empty() ? "home" : "search");
        const auto query = m_pages.front();
        const auto generation = m_generation;
        const std::weak_ptr<int> lifetime = m_lifetime;
        m_page_loading = true;
        m_tasks.enqueue(
            [session = m_session, query] { return read_page(session, query); },
            [this, generation, query, lifetime](TaskOutcome<YoutubeResult<MusicPage>> outcome) {
                if (lifetime.expired()) {
                    return;
                }
                finish_page(generation, query, std::move(outcome));
            }
        );
    }

    if (!m_suggestions_loading && m_suggestions_debounce.poll(now)) {
        const auto query = m_suggestion_query;
        const auto generation = m_generation;
        const std::weak_ptr<int> lifetime = m_lifetime;
        m_suggestions_loading = true;
        m_tasks.enqueue(
            [session = m_session, query] { return read_suggestions(session, query); },
            [this, generation, query, lifetime](TaskOutcome<YoutubeResult<MusicSuggestions>> outcome) {
                if (lifetime.expired()) {
                    return;
                }
                finish_suggestions(generation, query, std::move(outcome));
            }
        );
    }

    prune_searches();
    return changed;
}
