#pragma once

#include "../../utils/thread-pool.hpp"

#include <functional>
#include <string>
#include <unordered_map>

using ThumbnailFetcher = std::function<std::string(const std::string&)>;

class ThumbnailCache {
public:
    explicit ThumbnailCache(ThumbnailFetcher fetch = {});

    // repeated urls share one pending or completed download, an empty result means no image was downloaded
    std::shared_future<std::string> request(const std::string& url);

private:
    ThumbnailFetcher m_fetch;
    std::unordered_map<std::string, std::shared_future<std::string>> m_requests;
    ThreadPool m_workers;
};
