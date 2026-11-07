#pragma once

#include "error.hpp"
#include "../utils/json.hpp"

#include <stdexcept>
#include <utility>

struct YoutubeResponse {
    json data;
    long status;
};

template <typename T>
YoutubeResult<T> youtube_parse(YoutubeResult<YoutubeResponse> response) {
    if (!response) return std::unexpected(std::move(response.error()));
    try {
        return response->data.get<T>();
    } catch (const json::exception& error) {
        return youtube_error(
            YoutubeErrorKind::Parse, std::string("invalid YouTube response fields: ") + error.what(), response->status
        );
    } catch (const std::invalid_argument& error) {
        return youtube_error(YoutubeErrorKind::Parse, error.what(), response->status);
    }
}
