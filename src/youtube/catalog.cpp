#include "catalog.hpp"
#include "client.hpp"

#include <utility>

YoutubeMusicCatalog::YoutubeMusicCatalog(const YoutubeMusicClient& client) : m_client(client) {}

YoutubeResult<MusicPage>
YoutubeMusicCatalog::search(const MusicSearchRequest& request, const MusicRequestOptions& options) const {
    return page(MusicRoute::Search, "search", request, options);
}

YoutubeResult<MusicSuggestions>
YoutubeMusicCatalog::suggestions(std::string_view input, const MusicRequestOptions& options) const {
    return youtube_parse<MusicSuggestions>(m_client.post("music/get_search_suggestions", {{"input", input}}, options));
}

YoutubeResult<MusicPage>
YoutubeMusicCatalog::browse(const MusicBrowseRequest& request, const MusicRequestOptions& options) const {
    return page(MusicRoute::Browse, "browse", request, options);
}

YoutubeResult<MusicPage> YoutubeMusicCatalog::more(const MusicContinuation& continuation) const {
    if (continuation.route == MusicRoute::Next) {
        return youtube_error(YoutubeErrorKind::Api, "queue continuation must be read by YoutubeMusicQueue");
    }

    if (continuation.route == MusicRoute::Playlist) {
        return youtube_error(YoutubeErrorKind::Api, "playlist continuation must be read by YoutubeMusicPlaylists");
    }

    if (!continuation.owned_by(m_client.identity())) {
        return youtube_error(YoutubeErrorKind::Authentication, "continuation belongs to another channel");
    }

    return page(
        continuation.route, continuation.route == MusicRoute::Search ? "search" : "browse", json::object(),
        continuation.request_options(), {{"ctoken", continuation.token}, {"continuation", continuation.token}, {"type", "next"}}
    );
}

YoutubeResult<MusicPage> YoutubeMusicCatalog::page(
    MusicRoute route, std::string_view endpoint, json body, const MusicRequestOptions& options, cpr::Parameters parameters
) const {
    auto resolved = m_client.request_options(options);
    auto response = m_client.post(endpoint, std::move(body), resolved, {}, std::move(parameters));

    // continuation tokens belong to the visitor that received the page, even when signed out
    std::string visitor;
    const json::json_pointer visitor_path("/responseContext/visitorData");
    if (response && resolved.visitor_data.empty() && response->data.contains(visitor_path)) {
        const auto& issued = response->data.at(visitor_path);
        if (issued.is_string()) {
            issued.get_to(visitor);
            resolved.visitor_data = visitor;
        }
    }

    auto result = youtube_parse<MusicPage>(std::move(response));
    if (result) result->set_scope(route, resolved, m_client.identity());
    return result;
}
