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
    std::string body;
    std::chrono::milliseconds timeout{30000};
    std::chrono::milliseconds connect_timeout{10000};
    bool follow_redirects{false};
    std::stop_token stop;
    std::size_t response_limit = 0;
};

class HttpClient {
public:
    static HttpClient& instance();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) = delete;
    HttpClient& operator=(HttpClient&&) = delete;

    cpr::Response request(const HttpRequest& request) const;
    cpr::Response get(std::string_view url, cpr::Header headers = {}, cpr::Parameters parameters = {}) const;
    cpr::Response
    post_json(std::string_view url, const json& body, cpr::Header headers = {}, cpr::Parameters parameters = {}) const;

    static bool successful(const cpr::Response& response);

private:
    HttpClient() = default;
};
