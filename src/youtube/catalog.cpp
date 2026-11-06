#include "catalog.hpp"
#include "client.hpp"

#include <utility>

YoutubeMusicCatalog::YoutubeMusicCatalog(const YoutubeMusicClient& client) : m_client(client) {}

YoutubeResult<MusicPage>
YoutubeMusicCatalog::search(const MusicSearchRequest& request, const MusicRequestOptions& options) const {
    return page("search", request, options);
}

YoutubeResult<MusicSuggestions>
YoutubeMusicCatalog::suggestions(std::string_view input, const MusicRequestOptions& options) const {
    return youtube_parse<MusicSuggestions>(m_client.post("music/get_search_suggestions", {{"input", input}}, options));
}

YoutubeResult<MusicPage>
YoutubeMusicCatalog::browse(const MusicBrowseRequest& request, const MusicRequestOptions& options) const {
    return page("browse", request, options);
}

YoutubeResult<MusicPage> YoutubeMusicCatalog::more(const MusicContinuation& continuation) const {
    if (continuation.route == MusicRoute::Next) {
        return youtube_error(YoutubeErrorKind::Api, "queue continuation must be read by YoutubeMusicQueue");
    }
    if (continuation.route == MusicRoute::Playlist) {
        return youtube_error(YoutubeErrorKind::Api, "playlist continuation must be read by YoutubeMusicPlaylists");
    }
    if (continuation.auth != MusicAuth::Anonymous && continuation.identity != m_client.identity()) {
        return youtube_error(YoutubeErrorKind::Authentication, "continuation belongs to another channel");
    }
    return page(
        continuation.route == MusicRoute::Search ? "search" : "browse", json::object(),
        {continuation.client, continuation.auth, continuation.visitor_data},
        {{"ctoken", continuation.token}, {"continuation", continuation.token}, {"type", "next"}}
    );
}

YoutubeResult<MusicPage> YoutubeMusicCatalog::page(
    std::string_view endpoint, json body, const MusicRequestOptions& options, cpr::Parameters parameters
) const {
    const auto resolved = m_client.request_options(options);
    auto result = youtube_parse<MusicPage>(m_client.post(endpoint, std::move(body), resolved, {}, std::move(parameters)));
    if (result) result->set_scope(endpoint == "search" ? MusicRoute::Search : MusicRoute::Browse, resolved, m_client.identity());
    return result;
}
