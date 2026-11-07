#include "thumbnails.hpp"
#include "../../http/http-client.hpp"
#include "../../utils/log.hpp"
#include "../../utils/future.hpp"
#include "../../utils/thread-pool.hpp"
#include "../../youtube/schema/common.hpp"

#include <imgui-ui/resources/texture-registry.hpp>
#include <algorithm>
#include <utility>

const MusicThumbnail* select_thumbnail(std::span<const MusicThumbnail> thumbnails, float pixel_size) {
    const MusicThumbnail* selected = nullptr;
    for (const auto& candidate : thumbnails) {
        if (candidate.url.empty()) continue;
        const int extent = std::min(candidate.width, candidate.height);
        const int selected_extent = selected ? std::min(selected->width, selected->height) : 0;
        if (!selected || (selected_extent < pixel_size && extent > selected_extent) ||
            (extent >= pixel_size && extent < selected_extent)) {
            selected = &candidate;
        }
    }
    return selected;
}

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

ThumbnailCache::ThumbnailCache(ThreadPool& workers) : m_workers(workers) {}

std::shared_ptr<ThumbnailRequest> ThumbnailCache::request(const std::string& url) {
    auto& entry = m_requests[url];
    if (auto pending = entry.lock()) return pending;

    auto pending = std::make_shared<ThumbnailRequest>();
    entry = pending;
    auto task = m_workers.enqueue(
        [url, interest = std::weak_ptr(pending)] {
            const auto active = interest.lock();
            if (!active) return std::string{};
            return fetch_thumbnail(url);
        },
        TaskPriority::Background
    );
    pending->result = task.share();
    return pending;
}

ui::TextureHandle
ThumbnailCache::resolve(ui::TextureRegistry& textures, const std::string& url, std::shared_ptr<ThumbnailRequest>& pending) {
    if (!url.empty()) {
        if (pending && !future_ready(pending->result)) return {};

        auto image = textures.find_handle("thumbnail:" + url);
        if (image) {
            pending.reset();
            return image;
        }

        if (!pending) pending = request(url);
        if (!future_ready(pending->result)) return {};

        try {
            const auto& bytes = pending->result.get();
            if (!bytes.empty()) image = textures.acquire("thumbnail:" + url, std::string_view(bytes));
        } catch (const std::exception& error) {
            LOG_ERROR("[artwork] thumbnail failed: {} ({})", error.what(), url);
        }
        pending.reset();
        if (image) return image;
    }

    return textures.acquire("missing-artwork", std::filesystem::path("resources/icons/missing.svg"));
}

void ThumbnailCache::collect() {
    std::erase_if(m_requests, [](const auto& entry) { return entry.second.expired(); });
}
