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

    // keep the returned request alive while its artwork is needed
    std::shared_ptr<ThumbnailRequest> request(const std::string& url);
    // resolves cached artwork or starts a shared request for a visible cover
    ui::TextureHandle resolve(ui::TextureRegistry& textures, const std::string& url, std::shared_ptr<ThumbnailRequest>& pending);
    void collect();

private:
    std::unordered_map<std::string, std::weak_ptr<ThumbnailRequest>> m_requests;
    ThreadPool& m_workers;
};
