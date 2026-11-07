#pragma once

#include <chrono>
#include <future>

template <typename Future>
bool future_ready(const Future& task) {
    return task.valid() && task.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}
