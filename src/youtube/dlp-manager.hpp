#pragma once

#include <filesystem>
#include <mutex>
#include <string>

struct YoutubeDLPTools {
    std::filesystem::path executable;
    std::filesystem::path runtime;
};

class YoutubeDLPManager {
public:
    static YoutubeDLPManager& instance();
    // called from a worker, checks the latest release once per launch and reuses the installed version when the update fails
    YoutubeDLPTools prepare();

private:
    std::mutex m_mutex;
    YoutubeDLPTools m_tools;
};
