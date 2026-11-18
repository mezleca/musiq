#pragma once

#include <exception>
#include <expected>
#include <string>
#include <utility>

enum class YoutubeErrorKind {
    Authentication,
    Transport,
    Http,
    Api,
    Parse
};

struct YoutubeError {
    YoutubeErrorKind kind;
    long status = 0;
    int transport_code = 0;
    std::string message;
};

template <typename T>
using YoutubeResult = std::expected<T, YoutubeError>;

std::unexpected<YoutubeError> youtube_error(YoutubeErrorKind kind, std::string message, long status = 0);
std::unexpected<YoutubeError> youtube_transport_error(long status, int code);

template <typename T>
YoutubeResult<T> youtube_result(std::expected<YoutubeResult<T>, std::exception_ptr> result) {
    if (result) {
        return std::move(*result);
    }

    try {
        std::rethrow_exception(result.error());
    } catch (const std::exception& error) {
        return youtube_error(YoutubeErrorKind::Transport, error.what());
    } catch (...) {
        return youtube_error(YoutubeErrorKind::Transport, "unknown task failure");
    }
}
