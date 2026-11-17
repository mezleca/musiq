#include "thumbnails.hpp"
#include "../../utils/media.hpp"
#include "../../utils/path.hpp"
#include "../../utils/log.hpp"

#include <arbor/resources/texture-registry.hpp>
#include <algorithm>
#include <utility>

using namespace ui;
using namespace utils::media;

ThumbnailCache::ThumbnailCache() = default;

std::shared_ptr<ThumbnailRequest> ThumbnailCache::request(const std::string& url) {
    auto& entry = m_requests[url];
    if (auto pending = entry.lock()) return pending;

    auto pending = std::make_shared<ThumbnailRequest>();
    entry = pending;
    const std::weak_ptr<ThumbnailRequest> interest = pending;
    m_tasks.enqueue(
        [url, interest] {
            // skip queued downloads after every visible cover has released the request
            const auto active = interest.lock();
            if (!active) return std::string{};
            return fetch_thumbnail(url);
        },
        [interest, url](TaskOutcome<std::string> outcome) {
            const auto active = interest.lock();
            if (!active) return;
            if (!outcome) {
                try {
                    std::rethrow_exception(outcome.error());
                } catch (const std::exception& error) {
                    LOG_ERROR("[artwork] thumbnail failed: {} ({})", error.what(), url);
                } catch (...) {
                    LOG_ERROR("[artwork] thumbnail failed: unknown error ({})", url);
                }
                active->complete = true;
                return;
            }
            active->bytes = std::move(*outcome);
            active->complete = true;
        },
        TaskPriority::Background
    );
    return pending;
}

TextureHandle
ThumbnailCache::resolve(TextureRegistry& textures, const std::string& url, std::shared_ptr<ThumbnailRequest>& pending) {
    if (url.empty()) return textures.acquire("missing-artwork", utils::path::from_resources("icons/missing.svg"));
    if (pending && !pending->complete) return {};

    if (auto image = textures.find_handle("thumbnail:" + url)) {
        pending.reset();
        return image;
    }

    if (!pending) pending = request(url);
    if (!pending->complete) return {};

    if (!pending->bytes.empty()) {
        auto image = textures.acquire("thumbnail:" + url, std::string_view(pending->bytes));
        if (image) {
            pending.reset();
            return image;
        }
    }

    pending.reset();
    return textures.acquire("missing-artwork", utils::path::from_resources("icons/missing.svg"));
}

void ThumbnailCache::collect() {
    std::erase_if(m_requests, [](const auto& entry) { return entry.second.expired(); });
}
