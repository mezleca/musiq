#include "thumbnails.hpp"
#include "../../utils/media.hpp"
#include "../../utils/path.hpp"
#include "../../utils/log.hpp"

#include <arbor/backends/opengl/texture-loader.hpp>
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

    m_tasks.enqueue(
        pending->scope,
        [url, stop = pending->scope.token()]() -> std::unique_ptr<Texture> {
            const auto bytes = fetch_thumbnail(url, stop);
            if (bytes.empty() || stop.stop_requested()) {
                return {};
            }

            // decode without a graphics context, the texture uploads when drawn on the ui thread
            return OpenGLTextureLoader{}.load(std::string_view(bytes), url);
        },
        [pending = pending.get(), url](TaskOutcome<std::unique_ptr<Texture>> outcome) {
            if (!outcome) {
                try {
                    std::rethrow_exception(outcome.error());
                } catch (const std::exception& error) {
                    LOG_ERROR("[artwork] thumbnail failed: {} ({})", error.what(), url);
                } catch (...) {
                    LOG_ERROR("[artwork] thumbnail failed: unknown error ({})", url);
                }

                pending->complete = true;
                return;
            }

            pending->texture = std::move(*outcome);
            pending->complete = true;
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

    if (pending->texture) {
        auto image = textures.acquire("thumbnail:" + url, std::move(pending->texture));
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
