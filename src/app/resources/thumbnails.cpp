#include "thumbnails.hpp"
#include "../../utils/media.hpp"
#include "../../utils/path.hpp"
#include "../../utils/log.hpp"

#include <arbor/backends/opengl/texture-loader.hpp>
#include <arbor/resources/texture-registry.hpp>
#include <plutovg.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>

using namespace ui;
using namespace utils::media;

static std::optional<Color> average_cover_color(std::string_view bytes) {
    if (!std::in_range<int>(bytes.size())) return std::nullopt;

    const std::unique_ptr<plutovg_surface_t, decltype(&plutovg_surface_destroy)> surface(
        plutovg_surface_load_from_image_data(bytes.data(), static_cast<int>(bytes.size())), plutovg_surface_destroy
    );
    if (!surface) return std::nullopt;

    const auto* data = plutovg_surface_get_data(surface.get());
    const auto stride = plutovg_surface_get_stride(surface.get());
    const auto width = plutovg_surface_get_width(surface.get());
    const auto height = plutovg_surface_get_height(surface.get());

    std::uint64_t red = 0;
    std::uint64_t green = 0;
    std::uint64_t blue = 0;
    std::uint64_t alpha = 0;

    for (int y = 0; y < height; y += std::max(1, height / 24)) {
        for (int x = 0; x < width; x += std::max(1, width / 24)) {
            std::uint32_t pixel;
            std::memcpy(&pixel, data + y * stride + x * 4, sizeof(pixel));
            red += (pixel >> 16) & 255;
            green += (pixel >> 8) & 255;
            blue += pixel & 255;
            alpha += pixel >> 24;
        }
    }

    if (alpha == 0) return std::nullopt;

    // plutovg stores premultiplied channels, so transparent pixels carry less weight
    const auto weight = static_cast<float>(alpha);
    return rgb(static_cast<float>(red) / weight, static_cast<float>(green) / weight, static_cast<float>(blue) / weight);
}

ThumbnailCache::ThumbnailCache() = default;

std::shared_ptr<ThumbnailRequest> ThumbnailCache::request(const std::string& url) {
    auto& entry = m_requests[url];
    if (auto pending = entry.lock()) return pending;

    auto pending = std::make_shared<ThumbnailRequest>();
    entry = pending;

    m_tasks.enqueue(
        pending->scope,
        [url, stop = pending->scope.token()]() -> ThumbnailImage {
            const auto bytes = fetch_thumbnail(url, stop);
            if (bytes.empty() || stop.stop_requested()) {
                return {};
            }

            // decode without a graphics context, the texture uploads when drawn on the ui thread
            return {OpenGLTextureLoader{}.load(std::string_view(bytes), url), average_cover_color(bytes)};
        },
        [pending = pending.get(), url](TaskOutcome<ThumbnailImage> outcome) {
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

            pending->image = std::move(*outcome);
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

    if (pending->image.texture) {
        auto image = textures.acquire("thumbnail:" + url, std::move(pending->image.texture));
        if (image) {
            if (pending->image.accent) m_accents.insert_or_assign(url, ThumbnailAccent{image, *pending->image.accent});
            pending.reset();
            return image;
        }
    }

    pending.reset();
    return textures.acquire("missing-artwork", utils::path::from_resources("icons/missing.svg"));
}

std::optional<Color> ThumbnailCache::accent(const std::string& url) const {
    const auto found = m_accents.find(url);
    if (found == m_accents.end() || found->second.texture.expired()) return std::nullopt;
    return found->second.color;
}

void ThumbnailCache::collect() {
    std::erase_if(m_requests, [](const auto& entry) { return entry.second.expired(); });
    std::erase_if(m_accents, [](const auto& entry) { return entry.second.texture.expired(); });
}
