#pragma once

#include "../utils/json.hpp"

#include <optional>
#include <string_view>

class YoutubeMusicSession;

class YoutubeMusicClient {
public:
    explicit YoutubeMusicClient(const YoutubeMusicSession& session);
    std::optional<json> account_menu(std::string_view identity = {}) const;
    std::optional<json> accounts_list() const;

private:
    std::optional<json> post(std::string_view endpoint, json body = json::object(), std::string_view identity = {}) const;

    const YoutubeMusicSession& m_session;
};
