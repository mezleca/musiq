#include "task-queue.hpp"

#include <utility>

std::uint64_t TaskQueueState::generation() const {
    std::scoped_lock lock(m_mutex);
    return m_generation;
}

bool TaskQueueState::current(std::uint64_t generation) const {
    std::scoped_lock lock(m_mutex);
    return m_open && m_generation == generation;
}

void TaskQueueState::post(std::uint64_t generation, std::move_only_function<void()> callback) {
    std::scoped_lock lock(m_mutex);
    if (m_open && m_generation == generation) {
        m_callbacks.emplace(generation, std::move(callback));
    }
}

void TaskQueueState::drain() {
    std::queue<std::pair<std::uint64_t, std::move_only_function<void()>>> callbacks;
    // callbacks can enqueue more work or clear the queue without holding this mutex
    {
        std::scoped_lock lock(m_mutex);
        callbacks.swap(m_callbacks);
    }

    while (!callbacks.empty()) {
        auto [generation, callback] = std::move(callbacks.front());
        callbacks.pop();
        if (current(generation)) {
            callback();
        }
    }
}

void TaskQueueState::clear() {
    std::scoped_lock lock(m_mutex);
    // running work may finish, but its generation can no longer publish callbacks
    ++m_generation;
    m_callbacks = {};
}

void TaskQueueState::close() {
    std::scoped_lock lock(m_mutex);
    m_open = false;
    ++m_generation;
    m_callbacks = {};
}

TaskQueue::TaskQueue() : m_workers(ThreadPool::instance()), m_state(std::make_shared<TaskQueueState>()) {}

TaskQueue& TaskQueue::instance() {
    static TaskQueue queue;
    return queue;
}

TaskQueue::~TaskQueue() {
    m_state->close();
}

void TaskQueue::drain() {
    m_state->drain();
}

void TaskQueue::clear() {
    m_state->clear();
}
