#include "thread-pool.hpp"

#include <algorithm>

ThreadPool::ThreadPool() {
    const auto count = std::clamp(std::thread::hardware_concurrency(), 1u, 4u);
    for (unsigned index = 0; index < count; ++index) {
        m_workers.emplace_back([this](std::stop_token stop) {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock lock(m_mutex);
                    if (!m_ready.wait(lock, stop, [this] { return !m_tasks.empty(); })) return;
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }

                task();
            }
        });
    }
}
