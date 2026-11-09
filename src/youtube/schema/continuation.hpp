#pragma once

#include "request.hpp"

#include <optional>
#include <string>

enum class MusicRoute {
    Search,
    Browse,
    Next,
    Playlist
};

struct MusicContinuation {
    void set_scope(MusicRoute route, const MusicRequestOptions& options, std::string_view identity);
    MusicRequestOptions request_options() const;
    bool owned_by(std::string_view other) const;

    std::string token;
    MusicRoute route = MusicRoute::Browse;
    MusicClient client = MusicClient::WebRemix;
    MusicAuth auth = MusicAuth::Optional;
    std::string identity;
    std::string visitor_data;
    bool in_body = false;
};

std::optional<MusicContinuation> music_continuation(const json& data);
