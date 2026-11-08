#include "random.hpp"

#include <random>

static std::mt19937 engine;

void musiq::random::initialize() {
    engine.seed(std::random_device{}());
}

std::size_t musiq::random::random_index(std::size_t count) {
    return std::uniform_int_distribution<std::size_t>(0, count - 1)(engine);
}
