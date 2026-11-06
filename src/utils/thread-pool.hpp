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

class ThreadPool {
public:
    // shares queued jobs between up to four workers and waits for them to finish on destruction
    ThreadPool();

    template <typename F>
    auto enqueue(F&& callback) -> std::future<std::invoke_result_t<F>> {
        using Result = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<F>(callback));
        auto result = task->get_future();

        {
            std::lock_guard lock(m_mutex);
            m_tasks.emplace([task] { (*task)(); });
        }

        m_ready.notify_one();
        return result;
    }

private:
    std::mutex m_mutex;
    std::condition_variable_any m_ready;
    std::queue<std::function<void()>> m_tasks;
    std::vector<std::jthread> m_workers;
};
