#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

struct NavigationRoute {
    std::string page = "home";
    std::string query;

    bool operator==(const NavigationRoute&) const = default;
};

class NavigationHistory {
public:
    void visit(NavigationRoute route) {
        if (route == current()) return;
        m_pages.erase(m_pages.begin() + static_cast<std::ptrdiff_t>(m_position + 1), m_pages.end());
        m_pages.push_back(std::move(route));
        ++m_position;
    }

    bool back() {
        if (!can_back()) return false;
        --m_position;
        return true;
    }

    bool forward() {
        if (!can_forward()) return false;
        ++m_position;
        return true;
    }

    const NavigationRoute& current() const {
        return m_pages[m_position];
    }

    bool can_back() const {
        return m_position > 0;
    }

    bool can_forward() const {
        return m_position + 1 < m_pages.size();
    }

private:
    std::vector<NavigationRoute> m_pages{NavigationRoute{}};
    std::size_t m_position = 0;
};
