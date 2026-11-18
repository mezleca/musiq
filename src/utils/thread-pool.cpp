#include "thread-pool.hpp"

ThreadPool& ThreadPool::instance() {
    static ThreadPool pool;
    return pool;
}

ThreadPool::ThreadPool() {
    // separate workers keep background downloads from blocking foreground requests
    for (const auto priority : {TaskPriority::Foreground, TaskPriority::Background}) {
        auto* tasks = priority == TaskPriority::Foreground ? &m_tasks : &m_background;
        const unsigned count = priority == TaskPriority::Foreground ? 4 : 8;
        for (unsigned index = 0; index < count; ++index) {
            m_workers.emplace_back([this, tasks](std::stop_token stop) {
                while (true) {
                    std::move_only_function<void()> task;
                    {
                        std::unique_lock lock(m_mutex);
                        if (!m_ready.wait(lock, stop, [tasks] { return !tasks->empty(); })) {
                            return;
                        }
                        task = std::move(tasks->front());
                        tasks->pop();
                    }

                    task();
                }
            });
        }
    }
}
