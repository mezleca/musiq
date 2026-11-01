#pragma once

#include "../utils/json.hpp"

#include <cpr/cprtypes.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

class Api {
public:
    explicit Api(std::string base_url);

    std::optional<json> post(std::string_view endpoint, const json& body, cpr::Header headers = {}) const;
    static std::optional<std::string> download(std::string_view url);

private:
    std::string m_base_url;
};
