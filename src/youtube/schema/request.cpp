#include "request.hpp"
#include "../../constants.hpp"

#include <stdexcept>

static constexpr MusicClientProfile web{
    "WEB_REMIX",
    "67",
    MUSIC_CLIENT_VERSION,
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:140.0) Gecko/20100101 Firefox/140.0",
    true,
    {},
    {},
    {},
    {}
};

static constexpr MusicClientProfile vision{
    "VISIONOS",
    "101",
    "0.1",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Safari/605.1.15",
    false,
    "visionOS",
    "1.3.21O771",
    "Apple",
    "RealityDevice14,1"
};

const MusicClientProfile& music_client_profile(MusicClient client) {
    switch (client) {
        case MusicClient::WebRemix:
            return web;
        case MusicClient::VisionOs:
            return vision;
    }
    throw std::invalid_argument("unknown YouTube client");
}

void to_json(json& data, const MusicRequestContext& value) {
    const auto& profile = music_client_profile(value.client);

    data = {
        {"client", {{"clientName", profile.name}, {"clientVersion", profile.version}, {"hl", "en"}, {"gl", "US"}}},
        {"request", {{"internalExperimentFlags", json::array()}, {"useSsl", true}}},
        {"user", {{"lockedSafetyMode", false}}}
    };

    auto& client = data["client"];
    if (!profile.os_name.empty()) {
        client["osName"] = profile.os_name;
        client["osVersion"] = profile.os_version;
        client["deviceMake"] = profile.device_make;
        client["deviceModel"] = profile.device_model;
    }

    if (!value.visitor_data.empty()) client["visitorData"] = value.visitor_data;
    if (profile.login_supported && !value.identity.empty()) data["user"]["onBehalfOfUser"] = value.identity;
}
