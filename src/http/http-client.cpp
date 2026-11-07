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

    if (request.stop.stop_possible()) {
        session.SetProgressCallback(cpr::ProgressCallback{[stop = request.stop](auto, auto, auto, auto, intptr_t) {
            return !stop.stop_requested();
        }});
    }
    std::string bytes;
    if (request.response_limit != 0) {
        bytes.reserve(request.response_limit);
        // returning false aborts the transfer once the body exceeds response_limit
        session.SetWriteCallback(cpr::WriteCallback{[&bytes, limit = request.response_limit](std::string_view data, intptr_t) {
            if (data.size() > limit - bytes.size()) return false;
            bytes.append(data);
            return true;
        }});
    }

    if (!request.body.empty() || request.method == HttpMethod::Post || request.method == HttpMethod::Put ||
        request.method == HttpMethod::Patch) {
        session.SetBody(cpr::Body{request.body});
    }

    cpr::Response response;
    switch (request.method) {
        case HttpMethod::Get:
            response = session.Get();
            break;
        case HttpMethod::Post:
            response = session.Post();
            break;
        case HttpMethod::Put:
            response = session.Put();
            break;
        case HttpMethod::Patch:
            response = session.Patch();
            break;
        case HttpMethod::Delete:
            response = session.Delete();
            break;
        case HttpMethod::Head:
            response = session.Head();
            break;
        default:
            throw std::invalid_argument("unsupported HTTP method");
    }
    if (request.response_limit != 0) response.text = std::move(bytes);
    return response;
}

cpr::Response HttpClient::get(std::string_view url, cpr::Header headers, cpr::Parameters parameters) const {
    HttpRequest http_request;
    http_request.method = HttpMethod::Get;
    http_request.url = std::string(url);
    http_request.headers = std::move(headers);
    http_request.parameters = std::move(parameters);
    return request(http_request);
}

cpr::Response
HttpClient::post_json(std::string_view url, const json& body, cpr::Header headers, cpr::Parameters parameters) const {
    headers.try_emplace("Accept", "application/json");
    headers.try_emplace("Content-Type", "application/json");

    HttpRequest http_request;
    http_request.method = HttpMethod::Post;
    http_request.url = std::string(url);
    http_request.headers = std::move(headers);
    http_request.parameters = std::move(parameters);
    http_request.body = body.dump();
    return request(http_request);
}

bool HttpClient::successful(const cpr::Response& response) {
    return response.error.code == cpr::ErrorCode::OK && response.status_code >= 200 && response.status_code < 300;
}
