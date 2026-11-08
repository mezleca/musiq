#include "thread-pool.hpp"

#include <algorithm>

ThreadPool::ThreadPool() {
    // worker 0 serves foreground requests first
    // the others fall back to background artwork when the request queue is empty
    const auto count = std::clamp(std::thread::hardware_concurrency(), 1u, 4u);

    for (unsigned index = 0; index < count; ++index) {
        const bool background = count == 1 || index != 0;
        m_workers.emplace_back([this, background](std::stop_token stop) {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock lock(m_mutex);
                    if (!m_ready.wait(lock, stop, [this, background] {
                            return !m_tasks.empty() || (background && !m_background.empty());
                        })) {
                        return;
                    }
                    auto& tasks = m_tasks.empty() ? m_background : m_tasks;
                    task = std::move(tasks.front());
                    tasks.pop();
                }

                task();
            }
        });
    }
}
