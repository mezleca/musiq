#include "queue.hpp"
#include "client.hpp"

#include <utility>

YoutubeMusicQueue::YoutubeMusicQueue(const YoutubeMusicClient& client) : m_client(client) {}

YoutubeResult<MusicNextPage> YoutubeMusicQueue::next(const MusicNextRequest& request, const MusicRequestOptions& options) const {
    return read(request, options);
}

YoutubeResult<MusicNextPage> YoutubeMusicQueue::more(const MusicContinuation& continuation) const {
    if (continuation.route != MusicRoute::Next) return youtube_error(YoutubeErrorKind::Api, "not a queue continuation");
    return read({{"continuation", continuation.token}}, {continuation.client, continuation.auth, continuation.visitor_data});
}

YoutubeResult<MusicNextPage> YoutubeMusicQueue::read(json body, const MusicRequestOptions& options) const {
    const auto resolved = m_client.request_options(options);
    auto result = youtube_parse<MusicNextPage>(m_client.post("next", std::move(body), resolved));
    if (result && result->continuation) result->continuation->set_scope(MusicRoute::Next, resolved, m_client.identity());
    return result;
}
