#include "catalog-browser.hpp"
#include "catalog.hpp"
#include "client.hpp"
#include "playlists.hpp"
#include "../utils/log.hpp"

#include <format>
#include <algorithm>
#include <utility>

void CatalogBrowser::set_session(const YoutubeMusicSession& session) {
    m_pages.clear();

    m_changes = {};
    m_session = session;
    m_home = {};
    m_searches.clear();
    m_search_order.clear();
    m_query.clear();
    m_library.clear();
    m_created_playlist_ids.clear();
    refresh_library();

    suggest({});
}

void CatalogBrowser::refresh_library() {
    m_library_tasks.clear();
    m_pending_library.clear();
    m_library_ids.clear();
    m_library_tokens.clear();

    if (!m_session.user()) {
        m_library.clear();
        m_changes.library = true;
        return;
    }

    load_library_page();
}

void CatalogBrowser::load_library_page(std::optional<MusicContinuation> continuation) {
    m_tasks.enqueue(
        m_library_tasks,
        [session = m_session, continuation = std::move(continuation)] {
            const YoutubeMusicClient client(session);
            return continuation ? YoutubeMusicCatalog(client).more(*continuation) : YoutubeMusicPlaylists(client).library();
        },
        [this](auto result) { accept_library(youtube_result(std::move(result))); }
    );
}

void CatalogBrowser::add_created_playlist(MusicItem item) {
    const auto& id = item.endpoint.browse_id;
    if (std::ranges::none_of(m_library, [&id](const MusicItem& playlist) { return playlist.endpoint.browse_id == id; })) {
        m_created_playlist_ids.insert(id);
        m_library.insert(m_library.begin(), std::move(item));
        m_changes.library = true;
    }

    if (m_session.user()) {
        refresh_library();
    }
}

void CatalogBrowser::update_playlist_cover(std::string_view browse_id, const std::vector<MusicThumbnail>& thumbnails) {
    if (thumbnails.empty()) return;

    const auto matches = [browse_id](const MusicItem& item) { return item.endpoint.browse_id == browse_id; };
    if (const auto item = std::ranges::find_if(m_library, matches); item != m_library.end()) {
        const bool unchanged = std::ranges::equal(item->thumbnails, thumbnails, [](const auto& old, const auto& updated) {
            return old.url == updated.url && old.width == updated.width && old.height == updated.height;
        });
        if (!unchanged) {
            item->thumbnails = thumbnails;
            m_changes.library = true;
        }
    }
    if (const auto item = std::ranges::find_if(m_pending_library, matches); item != m_pending_library.end()) {
        item->thumbnails = thumbnails;
    }
}

void CatalogBrowser::remove_playlist(std::string_view browse_id) {
    const auto id = playlist_browse_id(browse_id);
    const auto matches = [&id](const MusicItem& item) { return item.endpoint.browse_id == id; };
    std::erase_if(m_library, matches);
    std::erase_if(m_pending_library, matches);
    m_created_playlist_ids.erase(id);
    m_library_ids.insert(id);
    m_changes.library = true;
}

const CatalogPageState& CatalogBrowser::search_state() const {
    static const CatalogPageState empty;
    const auto entry = m_searches.find(m_query);
    return entry == m_searches.end() ? empty : entry->second;
}

void CatalogBrowser::queue_page(std::string query) {
    auto& state = query.empty() ? m_home : m_searches.at(query);
    state.loading = true;
    state.error.clear();

    LOG_INFO("[catalog] loading {}", query.empty() ? "home" : "search");

    m_tasks.enqueue(
        m_pages,
        [session = m_session, query] {
            const YoutubeMusicClient client(session);
            const YoutubeMusicCatalog catalog(client);
            return query.empty() ? catalog.browse({.browse_id = "FEmusic_home"}) : catalog.search({.query = query});
        },
        [this, query](auto result) { accept_page(query, false, youtube_result(std::move(result))); }
    );
}

void CatalogBrowser::home(bool refresh) {
    if (!m_home.loading && (refresh || !m_home.page)) queue_page({});
}

bool CatalogBrowser::more_home() {
    if (m_home.loading || !m_home.error.empty() || !m_home.page || !m_home.page->continuation) return false;

    m_home.loading = true;

    m_tasks.enqueue(
        m_pages,
        [session = m_session, continuation = *m_home.page->continuation] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicCatalog(client).more(continuation);
        },
        [this](auto result) { accept_page({}, true, youtube_result(std::move(result))); }
    );

    return true;
}

void CatalogBrowser::search(std::string query, bool refresh) {
    m_query = std::move(query);
    if (m_query.empty()) return;

    std::erase(m_search_order, m_query);
    m_search_order.push_back(m_query);

    auto& state = m_searches[m_query];
    if (!state.loading && (refresh || !state.page)) queue_page(m_query);
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

void CatalogBrowser::accept_library(YoutubeResult<MusicPage> result) {
    if (!result) {
        m_pending_library.clear();
        LOG_WARN("[catalog] library playlists failed: {}", result.error().message);
        return;
    }

    auto continuation = std::move(result->continuation);
    for (auto& shelf : result->shelves) {
        for (auto& item : shelf.items) {
            const auto& id = item.endpoint.browse_id;
            if (id.starts_with("VL") && m_library_ids.insert(id).second) {
                if (item.thumbnails.empty()) {
                    const auto cached =
                        std::ranges::find_if(m_library, [&id](const MusicItem& entry) { return entry.endpoint.browse_id == id; });
                    if (cached != m_library.end()) item.thumbnails = cached->thumbnails;
                }
                m_pending_library.push_back(std::move(item));
            }
        }
        if (!continuation && shelf.continuation) {
            continuation = std::move(shelf.continuation);
        }
    }

    if (continuation) {
        if (m_library_tokens.insert(continuation->token).second) {
            load_library_page(std::move(continuation));
            return;
        }
        LOG_WARN("[catalog] repeated library continuation");
    }

    // keep newly created playlists visible until the library response includes them
    std::erase_if(m_created_playlist_ids, [this](const auto& id) { return m_library_ids.contains(id); });
    for (auto item = m_library.rbegin(); !m_created_playlist_ids.empty() && item != m_library.rend(); ++item) {
        if (m_created_playlist_ids.contains(item->endpoint.browse_id)) {
            m_pending_library.insert(m_pending_library.begin(), std::move(*item));
        }
    }

    // publishing partial pages would temporarily remove unchanged sidebar entries
    m_library = std::move(m_pending_library);
    m_changes.library = true;
}

void CatalogBrowser::accept_page(std::string query, bool append, YoutubeResult<MusicPage> result) {
    const bool home = query.empty();
    auto& state = home ? m_home : m_searches.at(query);
    state.loading = false;

    if (result) {
        if (append) {
            state.page->append(std::move(*result));
        } else {
            state.page = std::make_shared<MusicPage>(std::move(*result));
        }
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

    m_changes.home |= home;
    m_changes.search |= !home && query == m_query;
}

void CatalogBrowser::suggest(std::string query) {
    m_suggestion_tasks.clear();
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

    if (m_suggestions_debounce.poll(now)) {
        const auto query = m_suggestion_query;

        m_tasks.enqueue(
            m_suggestion_tasks,
            [session = m_session, query, stop = m_suggestion_tasks.token()] {
                const YoutubeMusicClient client(session);
                return YoutubeMusicCatalog(client).suggestions(query, {.auth = MusicAuth::Anonymous}, stop);
            },
            [this](auto outcome) {
                auto result = youtube_result(std::move(outcome));
                if (result) {
                    m_suggestions = std::move(*result);
                } else {
                    LOG_ERROR("[catalog] suggestions failed: {}", result.error().message);
                }
                m_changes.suggestions = true;
            }
        );
    }

    prune_searches();
    return changed;
}
