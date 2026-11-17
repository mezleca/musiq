#pragma once

#include "../../utils/task-queue.hpp"

#include <arbor/resources/texture.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace ui {
    class TextureRegistry;
}

struct ThumbnailRequest {
    std::string bytes;
    bool complete = false;
};

class ThumbnailCache {
public:
    ThumbnailCache();

    // resolves cached artwork or starts a shared request for a visible cover
    ui::TextureHandle resolve(ui::TextureRegistry& textures, const std::string& url, std::shared_ptr<ThumbnailRequest>& pending);
    void collect();

private:
    std::shared_ptr<ThumbnailRequest> request(const std::string& url);

    std::unordered_map<std::string, std::weak_ptr<ThumbnailRequest>> m_requests;
    TaskQueue& m_tasks = TaskQueue::instance();
};
