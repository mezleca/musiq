#include "thread-pool.hpp"

#include <algorithm>

ThreadPool::ThreadPool() {
    // with multiple workers, reserve one for user requests so artwork cannot occupy the whole pool
    const auto count = std::clamp(std::thread::hardware_concurrency(), 1u, 4u);

    for (unsigned index = 0; index < count; ++index) {
        const bool background = count == 1 || index != 0;
        m_workers.emplace_back([this, background](std::stop_token stop) {
            while (true) {
                std::move_only_function<void()> task;
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
