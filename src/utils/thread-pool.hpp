#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

enum class TaskPriority {
    Foreground,
    Background
};

class ThreadPool {
public:
    // up to four workers drain two queues, destruction stops intake, drains queued jobs and joins
    ThreadPool();

    template <typename F>
    auto enqueue(F&& callback, TaskPriority priority = TaskPriority::Foreground) -> std::future<std::invoke_result_t<F>> {
        using Result = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<F>(callback));
        auto result = task->get_future();

        {
            std::lock_guard lock(m_mutex);
            auto& tasks = priority == TaskPriority::Foreground ? m_tasks : m_background;
            tasks.emplace([task] { (*task)(); });
        }

        m_ready.notify_all();
        return result;
    }

private:
    std::mutex m_mutex;
    std::condition_variable_any m_ready;
    std::queue<std::function<void()>> m_tasks;
    std::queue<std::function<void()>> m_background;
    std::vector<std::jthread> m_workers;
};
