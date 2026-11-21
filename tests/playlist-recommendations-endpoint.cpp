#include "../src/youtube/client.hpp"
#include "../src/youtube/playlists.hpp"
#include "../src/youtube/playlist-browser.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <thread>

TEST_CASE("playlist recommendations load independently of actual entries", "[playlist-recommendations-endpoint]") {
    const auto* url = std::getenv("MUSIQ_RECOMMENDATIONS_API_URL");
    if (!url) SKIP("run through tests/python/playlist-recommendations.py");
    const YoutubeMusicConfig config{.api_url = url};
    YoutubeMusicSession session;
    REQUIRE(session.authenticate("SAPISID=local-secret", "local-sync-id", config));
    bool fail = false;
    SECTION("successful suggestions") {}
    SECTION("failed suggestions leave the playlist usable") { fail = true; }
    const MusicItem item{.endpoint = {.browse_id = fail ? "VLPLrecommendations-failure" : "VLPLrecommendations"}};
    PlaylistBrowser browser(config);
    REQUIRE(browser.load(session, item));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while ((browser.loading() || browser.recommendations_loading()) && std::chrono::steady_clock::now() < deadline) {
        TaskQueue::instance().drain();
        browser.poll(item);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE_FALSE(browser.loading());
    REQUIRE_FALSE(browser.recommendations_loading());
    CHECK(browser.error().empty());
    REQUIRE(browser.playlist().tracks.size() == 1);
    CHECK(browser.playlist().tracks.front().endpoint.video_id == "actual");
    if (fail) {
        CHECK_FALSE(browser.recommendations_error().empty());
        CHECK(browser.recommendations().empty());
    } else {
        CHECK(browser.recommendations_error().empty());
        REQUIRE(browser.recommendations().size() == 1);
        CHECK(browser.recommendations().front().endpoint.video_id == "suggested");
        CHECK(browser.recommendations().front().playlist_set_video_id.empty());
    }
    browser.invalidate();
    CHECK(browser.recommendations().empty());
}

TEST_CASE("playlist recommendation tokens retain channel scope", "[playlist-recommendations-endpoint]") {
    const YoutubeMusicSession session;
    const YoutubeMusicClient client(session);
    MusicContinuation continuation;
    continuation.set_scope(MusicRoute::Playlist, {.auth = MusicAuth::Required}, "other-channel");
    const auto result = YoutubeMusicPlaylists(client).recommendations("PLtest", continuation);
    REQUIRE_FALSE(result);
    CHECK(result.error().kind == YoutubeErrorKind::Authentication);
}
