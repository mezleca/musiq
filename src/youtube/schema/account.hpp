#pragma once

#include "../../utils/json.hpp"

#include <string>
#include <vector>

// ref: https://ytjs.dev/api/youtubei.js/namespaces/YTNodes/classes/AccountItem#properties
struct MusicIdentity {
    std::string name;
    std::string handle;
    std::string data_sync_id;
};

// ref: https://ytmusicapi.readthedocs.io/en/stable/reference/library.html#ytmusicapi.YTMusic.get_account_info
struct MusicAccountMenu {
    std::string name;
    std::string handle;
    std::string avatar_url;
    std::string data_sync_id;
    std::string data_sync_token;
};

struct MusicAccountsList {
    std::vector<MusicIdentity> identities;
};

void from_json(const json& data, MusicIdentity& value);
void from_json(const json& data, MusicAccountMenu& value);
void from_json(const json& data, MusicAccountsList& value);
