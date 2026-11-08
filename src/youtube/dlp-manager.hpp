#pragma once

#include <filesystem>

namespace dlp {

    struct Tools {
        std::filesystem::path executable;
        std::filesystem::path runtime;
    };

    // called from a worker, checks the latest release once per launch and reuses the installed version when the update fails
    Tools prepare();

} // namespace dlp
