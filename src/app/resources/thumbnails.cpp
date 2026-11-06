#include "thumbnails.hpp"
#include "../../http/http-client.hpp"
#include "../../utils/log.hpp"

#include <utility>

static std::string fetch_thumbnail(const std::string& url) {
    auto& http = HttpClient::instance();
    HttpRequest request;
    request.url = url;
    request.timeout = std::chrono::seconds(10);
    request.connect_timeout = std::chrono::seconds(5);
    request.follow_redirects = true;

    auto response = http.request(request);
    if (!HttpClient::successful(response)) {
        LOG_ERROR(
            "[artwork] thumbnail download failed (status={}, transport={}): {}", response.status_code,
            static_cast<int>(response.error.code), url
        );
        return {};
    }

    return std::move(response.text);
}

ThumbnailCache::ThumbnailCache(ThumbnailFetcher fetch) : m_fetch(fetch ? std::move(fetch) : fetch_thumbnail) {}

std::shared_future<std::string> ThumbnailCache::request(const std::string& url) {
    auto [entry, inserted] = m_requests.try_emplace(url);
    if (inserted) {
        auto task = m_workers.enqueue([fetch = m_fetch, url] { return fetch(url); });
        entry->second = task.share();
    }

    return entry->second;
}
