#include "continuation.hpp"

void MusicContinuation::set_scope(MusicRoute route, const MusicRequestOptions& options, std::string_view identity) {
    this->route = route;

    client = options.client;
    auth = options.auth;
    // anonymous continuations store no identity so they are not bound to a channel
    this->identity = auth == MusicAuth::Required ? identity : std::string_view{};
    visitor_data = options.visitor_data;
}

MusicRequestOptions MusicContinuation::request_options() const {
    return {client, auth, visitor_data};
}

bool MusicContinuation::owned_by(std::string_view other) const {
    return auth == MusicAuth::Anonymous || identity == other;
}

std::optional<MusicContinuation> music_continuation(const json& data) {
    const auto tokens = data.find("continuations");
    if (tokens == data.end() || tokens->empty()) return std::nullopt;

    // other continuation entries can lead the list, only the next page belongs to this route
    for (const auto& token : *tokens) {
        const auto radio = token.find("nextRadioContinuationData");
        const auto next = radio == token.end() ? token.find("nextContinuationData") : radio;
        if (next == token.end()) continue;

        const auto value = next->find("continuation");
        if (value == next->end()) continue;

        MusicContinuation continuation;
        continuation.token = value->get<std::string>();
        return continuation;
    }

    return std::nullopt;
}

std::optional<MusicContinuation> music_continuation_item(const json& item) {
    const json::json_pointer path("/continuationItemRenderer/continuationEndpoint");
    if (!item.contains(path)) return std::nullopt;

    const auto& endpoint = item.at(path);
    const json::json_pointer token("/continuationCommand/token");
    if (endpoint.contains(token)) {
        return MusicContinuation{.token = endpoint.at(token).get<std::string>(), .in_body = true};
    }

    const json::json_pointer commands("/commandExecutorCommand/commands");
    if (endpoint.contains(commands)) {
        for (const auto& command : endpoint.at(commands)) {
            if (!command.contains(token)) continue;
            const auto request = command.at("continuationCommand").value("request", std::string{});
            if (request == "CONTINUATION_REQUEST_TYPE_BROWSE" || request == "CONTINUATION_REQUEST_TYPE_SEARCH") {
                return MusicContinuation{.token = command.at(token).get<std::string>(), .in_body = true};
            }
        }
    }
    return std::nullopt;
}
