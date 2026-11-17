#include "../src/utils/task-queue.hpp"
#include "../src/utils/thread-pool.hpp"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <chrono>
#include <latch>
#include <stdexcept>
#include <thread>

TEST_CASE("thread pool drains work and preserves task errors", "[thread-pool]") {
    std::vector<std::future<int>> pending;
    auto& pool = ThreadPool::instance();
    for (int index = 0; index < 16; ++index) {
        pending.push_back(
            pool.enqueue([index] { return index; }, index % 2 ? TaskPriority::Foreground : TaskPriority::Background)
        );
    }

    auto failed = pool.enqueue([]() -> int { throw std::runtime_error("failed task"); });
    REQUIRE_THROWS_AS(failed.get(), std::runtime_error);

    for (int index = 0; index < 16; ++index) {
        REQUIRE(pending[index].get() == index);
    }
}

TEST_CASE("foreground requests remain available while artwork workers are busy", "[thread-pool]") {
    const auto workers = std::clamp(std::thread::hardware_concurrency(), 1u, 4u);
    if (workers == 1) SKIP("requires more than one worker");

    auto& pool = ThreadPool::instance();
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

TEST_CASE("task queue runs completions on the caller thread", "[task-queue]") {
    auto& tasks = TaskQueue::instance();
    tasks.clear();
    std::promise<void> work_finished;
    auto finished = work_finished.get_future();
    bool completed = false;
    std::thread::id completion_thread;
    const auto caller_thread = std::this_thread::get_id();

    tasks.enqueue(
        [&work_finished] {
            work_finished.set_value();
            return 42;
        },
        [&completed, &completion_thread](TaskOutcome<int> outcome) {
            REQUIRE(outcome);
            completed = true;
            completion_thread = std::this_thread::get_id();
        }
    );

    REQUIRE(finished.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!completed && std::chrono::steady_clock::now() < deadline) {
        tasks.drain();
        std::this_thread::yield();
    }
    REQUIRE(completed);
    REQUIRE(completion_thread == caller_thread);
}

TEST_CASE("clearing task queue drops pending completions", "[task-queue]") {
    auto& tasks = TaskQueue::instance();
    tasks.clear();
    std::promise<void> work_finished;
    auto finished = work_finished.get_future();
    bool completed = false;

    tasks.enqueue(
        [&work_finished] {
            work_finished.set_value();
            return 42;
        },
        [&completed](TaskOutcome<int>) { completed = true; }
    );

    REQUIRE(finished.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    tasks.clear();
    tasks.drain();
    REQUIRE_FALSE(completed);
}
