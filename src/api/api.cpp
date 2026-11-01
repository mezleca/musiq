#include "api.hpp"
#include "../utils/log.hpp"

#include <cpr/cpr.h>
#include <format>
#include <utility>

static bool successful(const cpr::Response& response) {
    if (response.error.code == cpr::ErrorCode::OK && response.status_code >= 200 && response.status_code < 300) {
        return true;
    }
    LOG_ERROR("[api] request failed: status={} error={}", response.status_code, response.error.message);
    return false;
}

Api::Api(std::string base_url) : m_base_url(std::move(base_url)) {}

std::optional<json> Api::post(std::string_view endpoint, const json& body, cpr::Header headers) const {
    headers.try_emplace("Accept", "application/json");
    headers.try_emplace("Content-Type", "application/json");

    const auto response = cpr::Post(
        cpr::Url{std::format("{}/{}", m_base_url, endpoint)}, headers, cpr::Body{body.dump()}, cpr::Redirect{false},
        cpr::Timeout{30000}, cpr::ConnectTimeout{10000}
    );

    if (!successful(response)) {
        return std::nullopt;
    }

    try {
        return json::parse(response.text);
    } catch (const json::parse_error&) {
        LOG_ERROR("[api] response contains invalid json");
        return std::nullopt;
    }
}

std::optional<std::string> Api::download(std::string_view url) {
    const auto response =
        cpr::Get(cpr::Url{std::string(url)}, cpr::Redirect{false}, cpr::Timeout{30000}, cpr::ConnectTimeout{10000});
    if (!successful(response)) return std::nullopt;
    return response.text;
}
