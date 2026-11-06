#include "debounce.hpp"

Debounce::Debounce(std::chrono::milliseconds delay) : m_delay(delay) {}

void Debounce::restart(std::chrono::steady_clock::time_point now) {
    m_deadline = now + m_delay;
}

void Debounce::cancel() {
    m_deadline.reset();
}

bool Debounce::poll(std::chrono::steady_clock::time_point now) {
    if (!m_deadline || now < *m_deadline) return false;
    m_deadline.reset();
    return true;
}
