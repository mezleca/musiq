#pragma once

#include "error.hpp"
#include "schema/catalog.hpp"

#include <cpr/parameters.h>

class YoutubeMusicClient;

class YoutubeMusicCatalog {
public:
    explicit YoutubeMusicCatalog(const YoutubeMusicClient& client);
    YoutubeResult<MusicPage> search(const MusicSearchRequest& request, const MusicRequestOptions& options = {}) const;
    YoutubeResult<MusicSuggestions> suggestions(std::string_view input, const MusicRequestOptions& options = {}) const;
    YoutubeResult<MusicPage> browse(const MusicBrowseRequest& request, const MusicRequestOptions& options = {}) const;
    YoutubeResult<MusicPage> more(const MusicContinuation& continuation) const;

private:
    YoutubeResult<MusicPage> page(
        MusicRoute route, std::string_view endpoint, json body, const MusicRequestOptions& options,
        cpr::Parameters parameters = {}
    ) const;

    const YoutubeMusicClient& m_client;
};
