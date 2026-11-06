#include "http-client.hpp"

#include <cpr/cpr.h>
#include <utility>
#include <stdexcept>

HttpClient& HttpClient::instance() {
    static HttpClient client;
    return client;
}

cpr::Response HttpClient::request(const HttpRequest& request) const {
    cpr::Session session;

    session.SetUrl(cpr::Url{request.url});
    session.SetHeader(request.headers);
    session.SetParameters(request.parameters);
    session.SetTimeout(cpr::Timeout{request.timeout});
    session.SetConnectTimeout(cpr::ConnectTimeout{request.connect_timeout});
    session.SetRedirect(cpr::Redirect{request.follow_redirects});

    if (!request.body.empty() || request.method == HttpMethod::Post || request.method == HttpMethod::Put ||
        request.method == HttpMethod::Patch) {
        session.SetBody(cpr::Body{request.body});
    }

    switch (request.method) {
        case HttpMethod::Get:
            return session.Get();
        case HttpMethod::Post:
            return session.Post();
        case HttpMethod::Put:
            return session.Put();
        case HttpMethod::Patch:
            return session.Patch();
        case HttpMethod::Delete:
            return session.Delete();
        case HttpMethod::Head:
            return session.Head();
    }

    throw std::invalid_argument("unsupported HTTP method");
}

cpr::Response HttpClient::get(std::string_view url, cpr::Header headers, cpr::Parameters parameters) const {
    return request({HttpMethod::Get, std::string(url), std::move(headers), std::move(parameters), {}});
}

cpr::Response
HttpClient::post_json(std::string_view url, const json& body, cpr::Header headers, cpr::Parameters parameters) const {
    headers.try_emplace("Accept", "application/json");
    headers.try_emplace("Content-Type", "application/json");
    return request({HttpMethod::Post, std::string(url), std::move(headers), std::move(parameters), body.dump()});
}

bool HttpClient::successful(const cpr::Response& response) {
    return response.error.code == cpr::ErrorCode::OK && response.status_code >= 200 && response.status_code < 300;
}
