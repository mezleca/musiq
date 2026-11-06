#include "../src/app/resources/thumbnails.hpp"
#include "../src/utils/thread-pool.hpp"

#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <stdexcept>

TEST_CASE("thumbnail cache shares downloads", "[thumbnails]") {
    std::atomic<int> downloads = 0;
    ThumbnailCache cache([&](const std::string&) {
        ++downloads;
        return "image bytes";
    });

    const auto first = cache.request("one");
    const auto repeated = cache.request("one");
    REQUIRE(first.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    REQUIRE(first.get() == "image bytes");
    REQUIRE(first.get() == repeated.get());
    REQUIRE(downloads == 1);
}

TEST_CASE("thread pool drains work and preserves task errors", "[thread-pool]") {
    std::vector<std::future<int>> pending;
    {
        ThreadPool pool;
        for (int index = 0; index < 16; ++index) {
            pending.push_back(pool.enqueue([index] { return index; }));
        }

        auto failed = pool.enqueue([]() -> int { throw std::runtime_error("failed task"); });
        REQUIRE_THROWS_AS(failed.get(), std::runtime_error);
    }

    for (int index = 0; index < 16; ++index) {
        REQUIRE(pending[index].get() == index);
    }
}
