#include "continuation.hpp"

void MusicContinuation::set_scope(MusicRoute route, const MusicRequestOptions& options, std::string_view identity) {
    this->route = route;
    client = options.client;
    auth = options.auth;
    this->identity = auth == MusicAuth::Required ? identity : std::string_view{};
    visitor_data = options.visitor_data;
}

std::optional<MusicContinuation> music_continuation(const json& data) {
    const auto tokens = data.find("continuations");
    if (tokens == data.end() || tokens->empty()) return std::nullopt;
    const auto& token = tokens->at(0);
    const auto radio = token.find("nextRadioContinuationData");
    const auto& next = radio == token.end() ? token.at("nextContinuationData") : *radio;
    MusicContinuation continuation;
    continuation.token = next.at("continuation").get<std::string>();
    return continuation;
}

void to_json(json& data, const MusicContinuation& value) {
    data = {
        {"token", value.token},
        {"route", static_cast<int>(value.route)},
        {"client", static_cast<int>(value.client)},
        {"auth", static_cast<int>(value.auth)},
        {"identity", value.identity},
        {"visitor_data", value.visitor_data},
        {"in_body", value.in_body}
    };
}
