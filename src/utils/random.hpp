#pragma once

#include <cstddef>

namespace musiq::random {
    void initialize();
    std::size_t random_index(std::size_t count);
} // namespace musiq::random
