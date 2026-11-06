#pragma once

#include "request.hpp"

#include <optional>
#include <string>

enum class MusicRoute {
    Search,
    Browse,
    Next
};

struct MusicContinuation {
    void set_scope(MusicRoute route, const MusicRequestOptions& options, std::string_view identity);

    std::string token;
    MusicRoute route = MusicRoute::Browse;
    MusicClient client = MusicClient::WebRemix;
    MusicAuth auth = MusicAuth::Optional;
    std::string identity;
    std::string visitor_data;
};

std::optional<MusicContinuation> music_continuation(const json& data);
void to_json(json& data, const MusicContinuation& value);
