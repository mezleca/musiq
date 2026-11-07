#pragma once

#include <cstdint>
#include <string>

struct AudioSource {
    std::string url;
    std::string user_agent;
    std::int64_t size = 0;
};
