#pragma once

#include "../../utils/json.hpp"

#include <string_view>

enum class MusicClient {
    WebRemix,
    VisionOs
};

enum class MusicAuth {
    Anonymous,
    Optional,
    Required
};

struct MusicRequestOptions {
    MusicClient client = MusicClient::WebRemix;
    MusicAuth auth = MusicAuth::Optional;
    std::string_view visitor_data{};
};

struct MusicRequestContext {
    MusicClient client = MusicClient::WebRemix;
    std::string_view visitor_data{};
    std::string_view identity{};
};

void to_json(json& data, const MusicRequestContext& value);
