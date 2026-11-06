#include "account.hpp"
#include "common.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

static std::string optional_text(const json& data, std::string_view key) {
    const auto field = data.find(key);
    return field == data.end() ? std::string{} : music_text(*field);
}

static std::string sync_id(std::string value) {
    const auto separator = value.find("||");
    if (separator == std::string::npos) return value;
    return separator == 0 ? value.substr(2) : value.substr(0, separator);
}

static std::string response_identity(const json& data) {
    const json::json_pointer path("/responseContext/mainAppWebResponseContext");
    if (!data.contains(path)) return {};
    return sync_id(data.at(path).value("datasyncId", std::string{}));
}

static const json& menu_renderer(const json& data) {
    for (const auto& action : data.at("actions")) {
        const auto popup = action.find("openPopupAction");
        if (popup != action.end()) {
            return popup->at("popup").at("multiPageMenuRenderer");
        }

        const auto menu = action.find("getMultiPageMenuAction");
        if (menu != action.end()) {
            return menu->at("menu").at("multiPageMenuRenderer");
        }
    }
    throw std::invalid_argument("missing account menu");
}

void from_json(const json& data, MusicIdentity& value) {
    MusicIdentity identity;
    identity.name = music_text(data.at("accountName"));
    identity.handle = optional_text(data, "channelHandle");

    std::string page_id;
    const json::json_pointer path("/serviceEndpoint/selectActiveIdentityEndpoint/supportedTokens");
    if (data.contains(path)) {
        for (const auto& token : data.at(path)) {
            const auto sync = token.find("datasyncIdToken");
            if (sync != token.end()) {
                identity.data_sync_id = sync_id(sync->at("datasyncIdToken").get<std::string>());
            }

            const auto page = token.find("pageIdToken");
            if (page != token.end()) page_id = page->at("pageId").get<std::string>();
        }
    }

    if (identity.data_sync_id.empty()) identity.data_sync_id = std::move(page_id);

    value = std::move(identity);
}

void to_json(json& data, const MusicIdentity& value) {
    data = {{"name", value.name}, {"handle", value.handle}, {"data_sync_id", value.data_sync_id}};
}

void from_json(const json& data, MusicAccountMenu& value) {
    const auto& header = menu_renderer(data).at("header").at("activeAccountHeaderRenderer");
    MusicAccountMenu account;
    account.name = music_text(header.at("accountName"));
    account.handle = optional_text(header, "channelHandle");
    account.data_sync_id = response_identity(data);

    const auto photo = header.find("accountPhoto");
    if (photo != header.end()) {
        const auto& thumbnails = photo->at("thumbnails");
        if (!thumbnails.empty()) account.avatar_url = thumbnails.back().at("url").get<std::string>();
    }

    if (account.name.empty()) throw std::invalid_argument("empty account name");

    value = std::move(account);
}

void to_json(json& data, const MusicAccountMenu& value) {
    data = {
        {"name", value.name}, {"handle", value.handle}, {"avatar_url", value.avatar_url}, {"data_sync_id", value.data_sync_id}
    };
}

static void append_account_group(const json& group, std::string_view selected_id, std::vector<MusicIdentity>& identities) {
    const auto items = group.find("accountItemSectionRenderer");
    if (items == group.end()) return;

    for (const auto& entry : items->at("contents")) {
        const auto item = entry.find("accountItem");
        if (item == entry.end() || item->value("isDisabled", false) || !item->value("hasChannel", true)) continue;

        auto identity = item->get<MusicIdentity>();
        if (identity.data_sync_id.empty() && item->value("isSelected", false)) identity.data_sync_id = selected_id;
        if (identity.name.empty() || identity.data_sync_id.empty()) continue;

        const auto duplicate = std::find_if(identities.begin(), identities.end(), [&](const MusicIdentity& existing) {
            return existing.data_sync_id == identity.data_sync_id;
        });
        if (duplicate == identities.end()) identities.push_back(std::move(identity));
    }
}

void from_json(const json& data, MusicAccountsList& value) {
    MusicAccountsList accounts;
    const auto selected_id = response_identity(data);
    bool found_section = false;

    for (const auto& section : menu_renderer(data).at("sections")) {
        const auto list = section.find("accountSectionListRenderer");
        if (list == section.end()) continue;

        found_section = true;
        for (const auto& group : list->at("contents")) {
            append_account_group(group, selected_id, accounts.identities);
        }
    }

    if (!found_section) throw std::invalid_argument("missing account list");
    value = std::move(accounts);
}

void to_json(json& data, const MusicAccountsList& value) {
    data = {{"identities", value.identities}};
}
