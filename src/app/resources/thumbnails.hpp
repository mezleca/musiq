#pragma once

#include <imgui-ui/resources/texture.hpp>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>

class ThreadPool;
namespace ui {
    class TextureRegistry;
}

struct ThumbnailRequest {
    std::shared_future<std::string> result;
};

class ThumbnailCache {
public:
    explicit ThumbnailCache(ThreadPool& workers);

    // callers keep pending downloads alive, abandoned queued requests skip network io
    std::shared_ptr<ThumbnailRequest> request(const std::string& url);
    // a nonempty pending request means the image is still loading, failures use the shared placeholder
    ui::TextureHandle resolve(ui::TextureRegistry& textures, const std::string& url, std::shared_ptr<ThumbnailRequest>& pending);
    void collect();

private:
    std::unordered_map<std::string, std::weak_ptr<ThumbnailRequest>> m_requests;
    ThreadPool& m_workers;
};
