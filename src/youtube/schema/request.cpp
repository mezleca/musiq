#include "request.hpp"
#include "../config.hpp"

void to_json(json& data, const MusicRequestContext& value) {
    const auto& profile = music_config::client_profile(value.client);
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
