#include "thumbnails.hpp"
#include "../../utils/media.hpp"
#include "../../utils/log.hpp"
#include "../../utils/future.hpp"
#include "../../utils/thread-pool.hpp"

#include <imgui-ui/resources/texture-registry.hpp>
#include <algorithm>
#include <utility>

using namespace ui;
using namespace media;

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

TextureHandle
ThumbnailCache::resolve(TextureRegistry& textures, const std::string& url, std::shared_ptr<ThumbnailRequest>& pending) {
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
