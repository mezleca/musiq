#include "../src/utils/thread-pool.hpp"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <algorithm>
#include <latch>

TEST_CASE("thread pool drains work and preserves task errors", "[thread-pool]") {
    std::vector<std::future<int>> pending;
    {
        ThreadPool pool;
        for (int index = 0; index < 16; ++index) {
            pending.push_back(
                pool.enqueue([index] { return index; }, index % 2 ? TaskPriority::Foreground : TaskPriority::Background)
            );
        }

        auto failed = pool.enqueue([]() -> int { throw std::runtime_error("failed task"); });
        REQUIRE_THROWS_AS(failed.get(), std::runtime_error);
    }

    for (int index = 0; index < 16; ++index) {
        REQUIRE(pending[index].get() == index);
    }
}

TEST_CASE("foreground requests remain available while artwork workers are busy", "[thread-pool]") {
    const auto workers = std::clamp(std::thread::hardware_concurrency(), 1u, 4u);
    if (workers == 1) SKIP("requires more than one worker");

    ThreadPool pool;
    std::latch started(workers - 1);
    std::promise<void> release;
    const auto gate = release.get_future().share();
    for (unsigned index = 1; index < workers; ++index) {
        pool.enqueue(
            [&started, gate] {
                started.count_down();
                gate.wait();
            },
            TaskPriority::Background
        );
    }
    started.wait();
    auto request = pool.enqueue([] { return 42; });
    const auto status = request.wait_for(std::chrono::seconds(1));
    release.set_value();
    REQUIRE(status == std::future_status::ready);
    REQUIRE(request.get() == 42);
}
