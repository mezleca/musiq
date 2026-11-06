#pragma once

#include "../utils/json.hpp"
#include "error.hpp"
#include "schema/account.hpp"
#include "schema/player.hpp"
#include "schema/request.hpp"
#include "response.hpp"

#include <cpr/parameters.h>
#include <cpr/cprtypes.h>

#include <string_view>

class YoutubeMusicSession;

class YoutubeMusicClient {
public:
    explicit YoutubeMusicClient(const YoutubeMusicSession& session);
    YoutubeResult<MusicAccountMenu> account_menu(std::string_view identity = {}) const;
    YoutubeResult<MusicAccountsList> accounts_list() const;
    YoutubeResult<MusicVisitorData> visitor_data() const;
    YoutubeResult<MusicPlayerData> player(const MusicPlayerRequest& request, const MusicRequestOptions& options = {}) const;
    MusicRequestOptions request_options(MusicRequestOptions options) const;
    const std::string& identity() const;
    YoutubeResult<YoutubeResponse> post(
        std::string_view endpoint, json body, const MusicRequestOptions& options, std::string_view identity = {},
        cpr::Parameters parameters = {}
    ) const;

private:
    YoutubeResult<cpr::Header> headers(const MusicRequestOptions& options) const;

    const YoutubeMusicSession& m_session;
};
