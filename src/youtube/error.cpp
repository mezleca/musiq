#include "error.hpp"

#include <utility>

std::unexpected<YoutubeError> youtube_error(YoutubeErrorKind kind, std::string message, long status) {
    return std::unexpected(YoutubeError{kind, status, 0, std::move(message)});
}

std::unexpected<YoutubeError> youtube_transport_error(long status, int code) {
    return std::unexpected(YoutubeError{YoutubeErrorKind::Transport, status, code, "request failed"});
}
