#include "http-client.hpp"

#include <cpr/cpr.h>
#include <utility>
#include <stdexcept>

cpr::Response http::request(const HttpRequest& request) {
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

cpr::Response http::get(std::string_view url, cpr::Header headers, cpr::Parameters parameters) {
    return request({.url = std::string(url), .headers = std::move(headers), .parameters = std::move(parameters)});
}

cpr::Response http::post_json(std::string_view url, const json& body, cpr::Header headers, cpr::Parameters parameters) {
    headers.try_emplace("Accept", "application/json");
    headers.try_emplace("Content-Type", "application/json");

    return request(
        {.method = HttpMethod::Post,
         .url = std::string(url),
         .headers = std::move(headers),
         .parameters = std::move(parameters),
         .body = body.dump()}
    );
}

bool http::successful(const cpr::Response& response) {
    return response.error.code == cpr::ErrorCode::OK && response.status_code >= 200 && response.status_code < 300;
}
