#include "random.hpp"

#include <random>

std::size_t utils::random::random_index(std::size_t count) {
    static std::mt19937 engine(std::random_device{}());
    return std::uniform_int_distribution<std::size_t>(0, count - 1)(engine);
}
