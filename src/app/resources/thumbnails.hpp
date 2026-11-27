#pragma once

#include "../../utils/task-queue.hpp"

#include <arbor/resources/texture.hpp>
#include <arbor/style/color.hpp>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace ui {
    class TextureRegistry;
}

struct ThumbnailImage {
    std::unique_ptr<ui::Texture> texture;
    std::optional<ui::Color> accent;
};

struct ThumbnailRequest {
    ThumbnailImage image;
    bool complete = false;
    TaskScope scope;
};

struct ThumbnailAccent {
    std::weak_ptr<ui::Texture> texture;
    ui::Color color;
};

class ThumbnailCache {
public:
    ThumbnailCache();

    // resolves cached artwork or starts a shared request for a visible cover
    ui::TextureHandle resolve(ui::TextureRegistry& textures, const std::string& url, std::shared_ptr<ThumbnailRequest>& pending);
    std::optional<ui::Color> accent(const std::string& url) const;
    void collect();

private:
    std::shared_ptr<ThumbnailRequest> request(const std::string& url);

    std::unordered_map<std::string, std::weak_ptr<ThumbnailRequest>> m_requests;
    std::unordered_map<std::string, ThumbnailAccent> m_accents;
    TaskQueue& m_tasks = TaskQueue::instance();
};
