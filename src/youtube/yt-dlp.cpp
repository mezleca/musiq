#include "yt-dlp.hpp"
#include "session.hpp"
#include "dlp-manager.hpp"
#include "../utils/json.hpp"
#include "../utils/log.hpp"

#include <boost/process/v2/popen.hpp>
#include <boost/process/v2/environment.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/steady_timer.hpp>

YoutubeResult<AudioSource>
resolve_with_ytdlp(const YoutubeMusicSession& session, const std::string& video_id, const std::string& reason) {
    namespace process = boost::process::v2;
    namespace asio = boost::asio;
    LOG_INFO("[player] using yt-dlp fallback: {}", reason);
    try {
        auto& manager = YoutubeDLPManager::instance();
        const auto tools = manager.prepare();
        const auto runtime_path = tools.runtime.u8string();
        const auto runtime = "deno:" + std::string(runtime_path.begin(), runtime_path.end());
        asio::io_context io;
        process::popen child(
            io, process::filesystem::path(tools.executable.native()),
            {"--ignore-config", "--no-warnings", "--no-playlist", "--socket-timeout", "10", "--retries", "1",
             "--extractor-retries", "1", "--js-runtimes", runtime, "--config-locations", "-", "--format",
             "bestaudio[protocol=https][filesize>0]", "--print",
             "{\"url\":%(url)j,\"user_agent\":%(http_headers.User-Agent)j,\"size\":%(filesize)j}",
             "https://www.youtube.com/watch?v=" + video_id}
        );
        std::string config;
        if (auto headers = session.authentication_headers()) {
            // stdin keeps cookies out of the process arguments and off disk, yt-dlp scopes them to youtube
            config += "--add-headers " + json("Cookie:" + headers->at("Cookie")).dump() + "\n";
            if (!session.data_sync_token().empty()) {
                config += "--extractor-args " + json("youtube:data_sync_id=" + session.data_sync_token()).dump() + "\n";
            }
        }
        asio::write(child.get_stdin(), asio::buffer(config));
        child.get_stdin().close();

        std::string output;
        bool timed_out = false;
        // a stalled extractor is terminated and its stdout closed so the pending read completes
        asio::steady_timer deadline(io, std::chrono::seconds(60));
        deadline.async_wait([&](const auto& error) {
            if (!error) {
                timed_out = true;
                boost::system::error_code ignored;
                child.terminate(ignored);
                child.get_stdout().close(ignored);
            }
        });
        asio::async_read(child.get_stdout(), asio::dynamic_buffer(output, 1024 * 1024), [&](const auto& error, std::size_t) {
            deadline.cancel();
            if (error && error != asio::error::eof) {
                boost::system::error_code ignored;
                child.terminate(ignored);
            }
        });
        io.run();
        const auto exit_code = child.wait();
        if (timed_out) return youtube_error(YoutubeErrorKind::Api, "yt-dlp resolution timed out");
        if (exit_code != 0) {
            return youtube_error(YoutubeErrorKind::Api, "yt-dlp could not resolve this track (see its error above)");
        }
        const auto data = json::parse(output);
        return AudioSource{
            data.at("url").get<std::string>(), data.at("user_agent").get<std::string>(), data.at("size").get<std::int64_t>()
        };
    } catch (const std::exception& error) {
        return youtube_error(YoutubeErrorKind::Api, std::string("yt-dlp fallback failed: ") + error.what());
    }
}
