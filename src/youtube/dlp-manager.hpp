#pragma once

#include <filesystem>

namespace dlp {

    struct Tools {
        std::filesystem::path executable;
        std::filesystem::path runtime;
    };

    // may download playback tools, call from a worker thread
    Tools prepare();

} // namespace dlp
