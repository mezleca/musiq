#pragma once

#include "../../utils/json.hpp"

#include <string_view>

enum class MusicClient {
    WebRemix,
    VisionOs
};

struct MusicClientProfile {
    std::string_view name;
    std::string_view id;
    std::string_view version;
    std::string_view user_agent;
    bool login_supported;
    std::string_view os_name;
    std::string_view os_version;
    std::string_view device_make;
    std::string_view device_model;
};

const MusicClientProfile& music_client_profile(MusicClient client);

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
