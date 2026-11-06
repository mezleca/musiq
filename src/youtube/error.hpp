#pragma once

#include <expected>
#include <string>

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
