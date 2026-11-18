#include "../src/utils/task-queue.hpp"
#include "../src/utils/thread-pool.hpp"

#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
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

TEST_CASE("network requests and artwork have independent concurrency", "[thread-pool][io]") {
    auto& pool = ThreadPool::instance();
    std::promise<void> release;
    const auto gate = release.get_future().share();
    std::promise<void> artwork_started;
    auto artwork_ready = artwork_started.get_future();
    std::promise<void> requests_started;
    auto requests_ready = requests_started.get_future();
    std::atomic<unsigned> artwork_count = 0;
    std::atomic<unsigned> request_count = 0;
    std::vector<std::future<void>> pending;

    for (unsigned index = 0; index < 8; ++index) {
        pending.push_back(pool.enqueue(
            [&] {
                if (++artwork_count == 8) artwork_started.set_value();
                gate.wait();
            },
            TaskPriority::Background
        ));
    }
    const auto artwork_status = artwork_ready.wait_for(std::chrono::seconds(2));
    for (unsigned index = 0; index < 4; ++index) {
        pending.push_back(pool.enqueue([&] {
            if (++request_count == 4) requests_started.set_value();
            gate.wait();
        }));
    }
    const auto request_status = requests_ready.wait_for(std::chrono::seconds(2));
    release.set_value();
    for (auto& task : pending)
        task.get();

    REQUIRE(artwork_status == std::future_status::ready);
    REQUIRE(request_status == std::future_status::ready);
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
