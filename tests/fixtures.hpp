#pragma once

#include "../src/utils/json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

inline json fixture(const char* name) {
    std::ifstream input(std::filesystem::path(MUSIQ_SOURCE_DIR) / "tests" / "fixtures" / name);
    REQUIRE(input.is_open());
    return json::parse(input);
}
