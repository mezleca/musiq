#include "../src/utils/thread-pool.hpp"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

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
