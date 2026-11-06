#pragma once

#include <chrono>
#include <optional>

class Debounce {
public:
    explicit Debounce(std::chrono::milliseconds delay);
    void restart(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    void cancel();
    bool poll(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

private:
    std::chrono::milliseconds m_delay;
    std::optional<std::chrono::steady_clock::time_point> m_deadline;
};
