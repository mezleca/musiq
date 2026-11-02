#include "session.hpp"
#include "client.hpp"
#include "../http/http-client.hpp"
#include "../utils/log.hpp"

#include <boost/hash2/sha1.hpp>
#include <chrono>
#include <format>
#include <utility>
#include <algorithm>

static std::string_view trim(std::string_view value) {
    const auto start = value.find_first_not_of(" \t");
    return start == std::string_view::npos ? std::string_view{} : value.substr(start, value.find_last_not_of(" \t") - start + 1);
}

static std::string_view cookie_sapisid(std::string_view cookies) {
    while (!cookies.empty()) {
        const auto end = cookies.find(';');
        const auto pair = cookies.substr(0, end);
        const auto separator = pair.find('=');

        if (separator != std::string_view::npos) {
            const auto name = trim(pair.substr(0, separator));
            const auto value = trim(pair.substr(separator + 1));

            if (!value.empty() && (name == "SAPISID" || name == "__Secure-3PAPISID")) {
                return value;
            }
        }

        if (end == std::string_view::npos) {
            break;
        }

        cookies.remove_prefix(end + 1);
    }

    return {};
}

// ref: https://github.com/SimoHypers/limusic/blob/d82372cdd3016e3c5e28ea8022596b7fefdf681a/crates/innertube/src/transport.rs#L769
static std::string sapisid_hash(std::string_view sapisid, int64_t timestamp) {
    const auto input = std::format("{} {} https://music.youtube.com", timestamp, sapisid);
    boost::hash2::sha1_160 sha1;
    sha1.update(input.data(), input.size());
    const auto digest = sha1.result();
    std::string hash;
    hash.reserve(digest.size() * 2);
    for (const auto byte : digest) {
        hash += std::format("{:02x}", byte);
    }
    return std::format("SAPISIDHASH {}_{}", timestamp, hash);
}

bool YoutubeMusicSession::has_session_cookie(std::string_view cookies) {
    return cookies.find_first_of("\r\n") == std::string_view::npos && cookies.find('\0') == std::string_view::npos &&
           !cookie_sapisid(cookies).empty();
}

std::optional<cpr::Header> YoutubeMusicSession::authentication_headers() const {
    if (m_sapisid.empty()) {
        return std::nullopt;
    }

    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return cpr::Header{
        {"Authorization", sapisid_hash(m_sapisid, std::chrono::duration_cast<std::chrono::seconds>(now).count())},
        {"Cookie", m_cookies}
    };
}

static const json* find_account_header(const json& node) {
    if (node.is_object() && node.contains("activeAccountHeaderRenderer")) {
        return &node["activeAccountHeaderRenderer"];
    }

    if (node.is_structured()) {
        for (const auto& child : node) {
            if (const auto* header = find_account_header(child)) {
                return header;
            }
        }
    }

    return nullptr;
}

static std::string account_text(const json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.contains("simpleText")) return value.at("simpleText").get<std::string>();

    std::string text;
    for (const auto& run : value.value("runs", json::array())) {
        text += run.at("text").get<std::string>();
    }

    return text;
}

static std::string sync_id(std::string value) {
    const auto separator = value.find("||");
    if (separator == std::string::npos) return value;
    return separator == 0 ? value.substr(2) : value.substr(0, separator);
}

static std::string response_identity(const json& response) {
    const json::json_pointer path("/responseContext/mainAppWebResponseContext");
    if (!response.contains(path)) return {};
    const auto& context = response.at(path);
    return sync_id(context.value("datasyncId", context.value("dataSyncId", std::string{})));
}

static void collect_identities(const json& node, std::string_view selected_id, std::vector<MusicIdentity>& identities) {
    if (node.is_object() && node.contains("accountItem")) {
        const auto& item = node.at("accountItem");
        if (item.value("isDisabled", false) || !item.value("hasChannel", true)) {
            return;
        }

        std::string id;
        std::string page_id;

        const json::json_pointer path("/serviceEndpoint/selectActiveIdentityEndpoint/supportedTokens");
        if (item.contains(path)) {
            for (const auto& token : item.at(path)) {
                if (token.contains("datasyncIdToken") || token.contains("dataSyncIdToken")) {
                    const auto& data = token.at(token.contains("datasyncIdToken") ? "datasyncIdToken" : "dataSyncIdToken");
                    id =
                        sync_id(data.value("datasyncIdToken", data.value("datasyncId", data.value("dataSyncId", std::string{}))));
                } else if (token.contains("pageIdToken")) {
                    page_id = token.at("pageIdToken").value("pageId", std::string{});
                }
            }
        }
        if (id.empty()) id = page_id;
        if (id.empty() && item.value("isSelected", false)) id = selected_id;

        const auto name = account_text(item.value("accountName", json::object()));
        if (!id.empty() && !name.empty() &&
            std::none_of(identities.begin(), identities.end(), [&](const MusicIdentity& identity) {
                return identity.data_sync_id == id;
            })) {
            identities.push_back({name, account_text(item.value("channelHandle", json::object())), std::move(id)});
        }

        return;
    }

    if (node.is_structured()) {
        for (const auto& child : node) {
            collect_identities(child, selected_id, identities);
        }
    }
}

bool YoutubeMusicSession::authenticate(std::string cookies, std::string_view identity) {
    if (!has_session_cookie(cookies)) {
        return false;
    }

    YoutubeMusicSession session;
    session.m_sapisid = cookie_sapisid(cookies);
    session.m_cookies = std::move(cookies);

    const auto response = YoutubeMusicClient(session).accounts_list();
    if (!response) {
        return false;
    }

    collect_identities(*response, response_identity(*response), session.m_identities);

    if (session.m_identities.empty()) {
        LOG_ERROR("[music] no selectable YouTube channels returned");
        return false;
    }

    if (identity.empty() && session.m_identities.size() == 1) {
        identity = session.m_identities.front().data_sync_id;
    }

    if (!identity.empty()) {
        session.select_identity(identity);
    }

    *this = std::move(session);
    return true;
}

bool YoutubeMusicSession::select_identity(std::string_view identity) {
    const auto selected = std::find_if(m_identities.begin(), m_identities.end(), [&](const MusicIdentity& item) {
        return item.data_sync_id == identity;
    });

    if (selected == m_identities.end()) {
        return false;
    }

    const auto response = YoutubeMusicClient(*this).account_menu(identity);
    if (!response) {
        return false;
    }

    try {
        const auto* header = find_account_header(*response);
        if (!header) {
            return false;
        }

        const auto handle = account_text(header->value("channelHandle", json::object()));
        if (response_identity(*response) != identity && (selected->handle.empty() || selected->handle != handle)) {
            LOG_ERROR("[music] selected channel was not confirmed");
            return false;
        }

        MusicUser user;
        user.name = account_text(header->at("accountName"));

        if (user.name.empty()) {
            return false;
        }

        if (!handle.empty()) {
            user.channel_url = std::format("https://www.youtube.com/{}", handle);
        }

        if (header->contains("accountPhoto")) {
            const auto& thumbnails = header->at("accountPhoto").at("thumbnails");
            if (!thumbnails.empty()) {
                user.avatar_url = thumbnails.back().at("url").get<std::string>();
            }
        }

        if (!user.avatar_url.empty()) {
            auto& http = HttpClient::instance();
            auto avatar = http.get(user.avatar_url);
            if (HttpClient::successful(avatar)) {
                user.avatar_data = std::move(avatar.text);
            }
        }

        m_user = std::move(user);
        m_identity = identity;
    } catch (const json::exception&) {
        LOG_ERROR("[music] invalid account menu response");
        return false;
    }

    return true;
}
