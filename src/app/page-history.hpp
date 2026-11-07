#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct PageRoute {
    std::string page = "home";
    std::string query;

    bool operator==(const PageRoute&) const = default;
};

class PageHistory {
public:
    // visiting a different route after going back replaces the forward branch
    void visit(PageRoute route);
    bool back();
    bool forward();

    const PageRoute& current() const {
        return m_pages[m_position];
    }

    bool can_back() const {
        return m_position > 0;
    }

    bool can_forward() const {
        return m_position + 1 < m_pages.size();
    }

private:
    std::vector<PageRoute> m_pages{PageRoute{}};
    std::size_t m_position = 0;
};
