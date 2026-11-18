#pragma once

#include "thread-pool.hpp"

#include <exception>
#include <expected>
#include <functional>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <type_traits>
#include <utility>

template <typename T>
using TaskOutcome = std::expected<T, std::exception_ptr>;

class TaskScope {
public:
    TaskScope() = default;
    ~TaskScope();
    TaskScope(const TaskScope&) = delete;
    TaskScope& operator=(const TaskScope&) = delete;

    void clear();
    std::stop_token token() const;

private:
    std::stop_source m_stop;
};

class TaskQueueState {
public:
    std::uint64_t generation() const;
    bool current(std::uint64_t generation) const;
    void post(std::uint64_t generation, std::move_only_function<void()> callback);
    void drain();
    void clear();
    void close();

private:
    mutable std::mutex m_mutex;
    std::queue<std::pair<std::uint64_t, std::move_only_function<void()>>> m_callbacks;
    std::uint64_t m_generation = 0;
    bool m_open = true;
};

class TaskQueue {
public:
    static TaskQueue& instance();

    ~TaskQueue();

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    template <typename Work, typename Completion>
    void enqueue(const TaskScope& scope, Work&& work, Completion&& completion, TaskPriority priority = TaskPriority::Foreground) {
        enqueue(std::forward<Work>(work), std::forward<Completion>(completion), priority, scope.token());
    }

    template <typename Work, typename Completion>
    void
    enqueue(Work&& work, Completion&& completion, TaskPriority priority = TaskPriority::Foreground, std::stop_token stop = {}) {
        using Result = std::invoke_result_t<Work>;

        const auto generation = m_state->generation();
        auto state = m_state;
        m_workers.enqueue(
            [state, generation, stop, work = std::forward<Work>(work),
             completion = std::forward<Completion>(completion)]() mutable {
                if (stop.stop_requested() || !state->current(generation)) {
                    return;
                }

                TaskOutcome<Result> outcome = [&]() -> TaskOutcome<Result> {
                    try {
                        if constexpr (std::is_void_v<Result>) {
                            std::invoke(work);
                            return {};
                        } else {
                            return std::invoke(work);
                        }
                    } catch (...) {
                        return std::unexpected(std::current_exception());
                    }
                }();

                state->post(generation, [stop, completion = std::move(completion), outcome = std::move(outcome)]() mutable {
                    // the owner may have been destroyed while the worker was running
                    if (!stop.stop_requested()) {
                        std::invoke(completion, std::move(outcome));
                    }
                });
            },
            priority
        );
    }

    void drain();
    void clear();

private:
    TaskQueue();

    ThreadPool& m_workers;
    std::shared_ptr<TaskQueueState> m_state;
};
