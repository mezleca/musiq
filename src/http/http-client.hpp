#pragma once

#include "../utils/json.hpp"

#include <cpr/parameters.h>
#include <cpr/response.h>
#include <chrono>
#include <string>
#include <string_view>
#include <stop_token>

enum class HttpMethod {
    Get,
    Post,
    Put,
    Patch,
    Delete,
    Head
};

struct HttpRequest {
    HttpMethod method{HttpMethod::Get};
    std::string url;
    cpr::Header headers;
    cpr::Parameters parameters;
    std::string body{};
    std::chrono::milliseconds timeout{30000};
    std::chrono::milliseconds connect_timeout{10000};
    bool follow_redirects{false};
    std::stop_token stop{};
    // maximum response bytes, zero disables the limit
    std::size_t response_limit = 0;
};

namespace http {
    cpr::Response request(const HttpRequest& request);
    cpr::Response get(std::string_view url, cpr::Header headers = {}, cpr::Parameters parameters = {});
    cpr::Response post_json(std::string_view url, const json& body, cpr::Header headers = {}, cpr::Parameters parameters = {});
    bool successful(const cpr::Response& response);
} // namespace http
