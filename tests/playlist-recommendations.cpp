#include "../src/youtube/schema/playlist.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("playlist recommendations have a separate section continuation", "[youtube][playlist-recommendations]") {
    const json data = {
        {"contents", {{"twoColumnBrowseResultsRenderer", {
            {"secondaryContents", {{"sectionListRenderer", {
                {"contents", json::array({{{"musicPlaylistShelfRenderer", {
                    {"playlistId", "PLtest"}, {"contents", json::array()},
                    {"continuations", json::array({{{"nextContinuationData", {{"continuation", "tracks"}}}}})}
                }}}})},
                {"continuations", json::array({{{"nextContinuationData", {{"continuation", "suggestions"}}}}})}
            }}}}
        }}}}
    };
    const auto playlist = data.get<MusicPlaylist>();
    REQUIRE(playlist.continuation);
    CHECK(playlist.continuation->token == "tracks");
    REQUIRE(playlist.recommendations_continuation);
    CHECK(playlist.recommendations_continuation->token == "suggestions");
    CHECK(playlist.tracks.empty());
}

TEST_CASE("playlist suggestions parse only the recommendation shelf", "[youtube][playlist-recommendations]") {
    const json data = json::parse(R"({
        "continuationContents": {"sectionListContinuation": {"contents": [
            {"musicShelfRenderer": {"contents": [
                {"musicResponsiveListItemRenderer": {
                    "flexColumns": [{"musicResponsiveListItemFlexColumnRenderer": {
                        "text": {"runs": [{"text": "Suggested song", "navigationEndpoint": {
                            "watchEndpoint": {"videoId": "suggested-video"}
                        }}]}
                    }}],
                    "playlistItemData": {"playlistSetVideoId": "to_be_updated_by_client"}
                }}
            ]}},
            {"musicCarouselShelfRenderer": {"contents": []}}
        ]}}
    })");
    const auto recommendations = data.get<MusicPlaylistRecommendations>();
    REQUIRE(recommendations.tracks.size() == 1);
    CHECK(recommendations.tracks.front().endpoint.video_id == "suggested-video");
    CHECK(recommendations.tracks.front().playlist_set_video_id.empty());
}
