#include "page-history.hpp"

#include <utility>

void PageHistory::visit(PageRoute route) {
    if (route == current()) return;

    m_pages.erase(m_pages.begin() + static_cast<std::ptrdiff_t>(m_position + 1), m_pages.end());
    m_pages.push_back(std::move(route));
    ++m_position;
}

bool PageHistory::back() {
    if (!can_back()) return false;
    --m_position;
    return true;
}

bool PageHistory::forward() {
    if (!can_forward()) return false;
    ++m_position;
    return true;
}
