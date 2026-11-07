#include "dlp-manager.hpp"
#include "../http/http-client.hpp"
#include "../utils/system.hpp"
#include "../utils/log.hpp"

#include <boost/hash2/sha2.hpp>
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <format>

static std::string download_tool(const std::string& url) {
    HttpRequest request;
    request.url = url;
    request.headers = {{"User-Agent", "musiq"}};
    request.follow_redirects = true;
    request.timeout = std::chrono::seconds(url.starts_with("https://api.github.com/") ? 5 : 120);
    request.connect_timeout = std::chrono::seconds(5);
    request.response_limit = 128 * 1024 * 1024;
    auto& http = HttpClient::instance();
    auto response = http.request(request);
    if (!HttpClient::successful(response)) {
        throw std::runtime_error(std::format("tool download failed (HTTP {})", response.status_code));
    }
    return std::move(response.text);
}

static std::filesystem::path install_tool(const std::string& repository, const std::string& name, const std::string& binary) {
    const auto root = utils::get_app_dir() / "tools" / std::filesystem::path(binary).stem();
    std::string previous;
    {
        std::ifstream current(root / "current");
        current >> previous;
    }
    try {
        const auto release = json::parse(download_tool("https://api.github.com/repos/" + repository + "/releases/latest"));
        const auto asset =
            std::ranges::find(release.at("assets"), name, [](const json& item) { return item.at("name").get<std::string>(); });
        if (asset == release.at("assets").end()) throw std::runtime_error("release has no binary for this platform");

        const auto digest = asset->at("digest").get<std::string>();
        if (!digest.starts_with("sha256:") || digest.size() != 71) throw std::runtime_error("release has no SHA256 checksum");
        const auto version = digest.substr(7);
        const auto path = root / version / binary;
        // the digest names the version directory, so an already installed binary skips the download
        if (!std::filesystem::exists(path)) {
            LOG_INFO("[tools] downloading {} {}", binary, release.at("tag_name").get<std::string>());
            auto bytes = download_tool(asset->at("browser_download_url").get<std::string>());
            // verify the bytes against the digest before anything reaches the final path
            boost::hash2::sha2_256 hash;
            hash.update(bytes.data(), bytes.size());
            if (boost::hash2::to_string(hash.result()) != version) throw std::runtime_error("tool checksum mismatch");
            std::unique_ptr<void, decltype(&mz_free)> extracted(nullptr, mz_free);
            std::string_view content(bytes);
            if (name.ends_with(".zip")) {
                mz_zip_archive zip{};
                if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
                    throw std::runtime_error("invalid tool archive");
                }
                std::size_t length = 0;
                extracted.reset(mz_zip_reader_extract_file_to_heap(&zip, binary.c_str(), &length, 0));
                mz_zip_reader_end(&zip);
                if (!extracted) throw std::runtime_error("tool binary missing from archive");
                content = {static_cast<const char*>(extracted.get()), length};
            }
            std::filesystem::create_directories(path.parent_path());
            auto temporary = path;
            temporary += ".part";
            std::ofstream file(temporary, std::ios::binary);
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file.write(content.data(), static_cast<std::streamsize>(content.size()));
            file.close();
#ifndef _WIN32
            std::filesystem::permissions(temporary, std::filesystem::perms::owner_all);
#endif
            // publish the checked archive member atomically after the complete write
            std::filesystem::rename(temporary, path);
        }
        std::ofstream(root / "current") << version;
        return path;
    } catch (const std::exception& error) {
        const auto cached = root / previous / binary;
        if (previous.empty() || !std::filesystem::exists(cached)) throw;
        LOG_WARN("[tools] update failed, using cached {}: {}", binary, error.what());
        return cached;
    }
}

YoutubeDLPManager& YoutubeDLPManager::instance() {
    static YoutubeDLPManager manager;
    return manager;
}

YoutubeDLPTools YoutubeDLPManager::prepare() {
    const std::lock_guard lock(m_mutex);
    if (!m_tools.executable.empty()) return m_tools;
#if defined(_M_ARM64) || defined(__aarch64__)
    const std::string architecture = "aarch64";
#else
    const std::string architecture = "x86_64";
#endif
#ifdef _WIN32
    const std::string executable = "yt-dlp.exe";
    const std::string asset = architecture == "aarch64" ? "yt-dlp_arm64.exe" : executable;
    const std::string runtime = "deno.exe";
    const std::string runtime_asset = "deno-" + architecture + "-pc-windows-msvc.zip";
#else
    const std::string executable = "yt-dlp";
    const std::string asset = architecture == "aarch64" ? "yt-dlp_linux_aarch64" : "yt-dlp_linux";
    const std::string runtime = "deno";
    const std::string runtime_asset = "deno-" + architecture + "-unknown-linux-gnu.zip";
#endif
    YoutubeDLPTools tools;
    tools.executable = install_tool("yt-dlp/yt-dlp", asset, executable);
    tools.runtime = install_tool("denoland/deno", runtime_asset, runtime);
    m_tools = std::move(tools);
    return m_tools;
}
