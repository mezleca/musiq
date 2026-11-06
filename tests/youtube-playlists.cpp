#include "../src/youtube/playlists.hpp"
#include "../src/youtube/catalog.hpp"
#include "../src/youtube/client.hpp"
#include "../src/youtube/session.hpp"
#include "fixtures.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <stdexcept>

static const json::json_pointer
    header_path("/contents/twoColumnBrowseResultsRenderer/tabs/0/tabRenderer/content/sectionListRenderer/contents/0");
static const json::json_pointer shelf_path(
    "/contents/twoColumnBrowseResultsRenderer/secondaryContents/sectionListRenderer/contents/0/musicPlaylistShelfRenderer"
);

TEST_CASE("playlist reads normalize browse ids and reject invalid scopes before sending requests", "[youtube][playlist]") {
    REQUIRE(json(MusicPlaylistRequest{"PLexample"}).at("browseId") == "VLPLexample");
    REQUIRE(json(MusicPlaylistRequest{"VLPLexample"}).at("browseId") == "VLPLexample");

    const YoutubeMusicSession session;
    const YoutubeMusicClient client(session);
    const YoutubeMusicPlaylists playlists(client);
    const auto private_list = playlists.get("PLprivate", {.auth = MusicAuth::Required});
    REQUIRE_FALSE(private_list);
    REQUIRE(private_list.error().kind == YoutubeErrorKind::Authentication);

    const MusicContinuation token{
        .token = "private-token", .route = MusicRoute::Playlist, .auth = MusicAuth::Required, .identity = "other-channel"
    };
    const auto next = playlists.more(token);
    REQUIRE_FALSE(next);
    REQUIRE(next.error().kind == YoutubeErrorKind::Authentication);
    REQUIRE_FALSE(YoutubeMusicCatalog(client).more(token));
}

TEST_CASE("playlist details preserve metadata repeated tracks and unavailable entries", "[youtube][playlist]") {
    const auto playlist = fixture("playlist.json").get<MusicPlaylist>();
    REQUIRE(playlist.id == "PLexample");
    REQUIRE(playlist.title == "Example playlist");
    REQUIRE(playlist.description == "A short description.");
    REQUIRE(playlist.author.endpoint.browse_id == "UCCREATOR");
    REQUIRE(playlist.year == "2026");
    REQUIRE(playlist.track_count == 1234);
    REQUIRE(playlist.views == "2,345 views");
    REQUIRE(playlist.duration == "6+ hours");
    REQUIRE(playlist.privacy == "PUBLIC");
    REQUIRE_FALSE(playlist.owned);

    REQUIRE(playlist.tracks.size() == 2);
    REQUIRE(playlist.tracks[0].endpoint.video_id == playlist.tracks[1].endpoint.video_id);
    REQUIRE(playlist.tracks[0].playlist_set_video_id == "entry-one");
    REQUIRE(playlist.tracks[1].playlist_set_video_id == "entry-two");
    REQUIRE(playlist.tracks[0].available);
    REQUIRE_FALSE(playlist.tracks[1].available);
    REQUIRE(playlist.tracks[0].album.endpoint.browse_id == "MPREalbum");
    REQUIRE(playlist.tracks[1].duration == "3:21");
    REQUIRE(playlist.continuation->token == "next+/=");
    REQUIRE(playlist.continuation->in_body);
}

TEST_CASE("owned empty playlists retain their privacy and header", "[youtube][playlist]") {
    auto data = fixture("playlist.json");
    auto header = data.at(header_path).at("musicResponsiveHeaderRenderer");
    header["secondSubtitle"] = {{"runs", {{{"text", "0 songs"}}}}};
    auto& entry = data.at(header_path);
    entry.clear();
    auto& editable = entry["musicEditablePlaylistDetailHeaderRenderer"];
    editable["header"]["musicResponsiveHeaderRenderer"] = std::move(header);
    editable["editHeader"]["musicPlaylistEditHeaderRenderer"]["privacy"] = "PRIVATE";
    data.at(shelf_path).erase("contents");

    const auto playlist = data.get<MusicPlaylist>();
    REQUIRE(playlist.owned);
    REQUIRE(playlist.privacy == "PRIVATE");
    REQUIRE(playlist.title == "Example playlist");
    REQUIRE(playlist.track_count == 0);
    REQUIRE(playlist.tracks.empty());
    REQUIRE_FALSE(playlist.continuation);
}

TEST_CASE("playlist rows permit empty album columns", "[youtube][playlist]") {
    auto data = fixture("playlist.json");
    auto& row = data.at(shelf_path)["contents"][1]["musicResponsiveListItemRenderer"];
    row["flexColumns"][3]["musicResponsiveListItemFlexColumnRenderer"]["text"] = json::object();
    const auto playlist = data.get<MusicPlaylist>();
    REQUIRE(playlist.tracks[1].album.text.empty());
    REQUIRE(playlist.tracks[1].duration == "3:21");
}

TEST_CASE("playlist facepiles distinguish authors from collaborators", "[youtube][playlist]") {
    auto data = fixture("playlist.json");
    auto& header = data.at(header_path).at("musicResponsiveHeaderRenderer");
    header.erase("straplineTextOne");
    auto& facepile = header["facepile"]["avatarStackViewModel"];
    facepile["text"]["content"] = "New creator";
    facepile["rendererContext"]["commandContext"]["onTap"]["innertubeCommand"]["browseEndpoint"]["browseId"] = "UCNEW";
    REQUIRE(data.get<MusicPlaylist>().author.endpoint.browse_id == "UCNEW");

    facepile["rendererContext"].erase("commandContext");
    facepile["rendererContext"]["accessibilityContext"]["label"] = "Creator and 2 others";
    const auto playlist = data.get<MusicPlaylist>();
    REQUIRE(playlist.author.text.empty());
    REQUIRE(playlist.collaborators == "Creator and 2 others");
}

TEST_CASE("legacy playlists and audio lists reuse the same tracks", "[youtube][playlist]") {
    auto data = fixture("playlist.json");
    auto header = data.at(header_path).at("musicResponsiveHeaderRenderer");
    header["description"] = {{"simpleText", "Legacy description"}};
    auto image = header["thumbnail"]["musicThumbnailRenderer"];
    header["thumbnail"] = {{"croppedSquareThumbnailRenderer", std::move(image)}};
    data["header"] = {{"musicDetailHeaderRenderer", std::move(header)}};
    auto shelf = data.at(shelf_path);
    shelf["contents"].erase(shelf["contents"].size() - 1);
    shelf["continuations"] = {{{"nextContinuationData", {{"continuation", "legacy+/="}}}}};
    auto& body = data["contents"]["twoColumnBrowseResultsRenderer"];
    body["tabs"][0]["tabRenderer"]["content"] = body["secondaryContents"];
    body["tabs"][0]["tabRenderer"]["content"]["sectionListRenderer"]["contents"][0]["musicPlaylistShelfRenderer"] =
        std::move(shelf);
    data["contents"]["singleColumnBrowseResultsRenderer"] = std::move(body);
    data["contents"].erase("twoColumnBrowseResultsRenderer");

    const auto playlist = data.get<MusicPlaylist>();
    REQUIRE(playlist.description == "Legacy description");
    REQUIRE_FALSE(playlist.thumbnails.empty());
    REQUIRE(playlist.tracks.size() == 2);
    REQUIRE(playlist.continuation->token == "legacy+/=");
    REQUIRE_FALSE(playlist.continuation->in_body);

    auto audio = fixture("playlist.json");
    audio["contents"]["twoColumnBrowseResultsRenderer"].erase("tabs");
    auto& rows = audio.at(shelf_path).at("contents");
    rows.erase(rows.size() - 1);
    const auto album = audio.get<MusicPlaylist>();
    REQUIRE(album.title == "Example album");
    REQUIRE(album.track_count == 2);
    REQUIRE_FALSE(album.continuation);
}

TEST_CASE("playlist pagination handles action envelopes and legacy shelves", "[youtube][playlist]") {
    auto data = fixture("playlist-continuation.json");
    auto page = data.get<MusicPlaylistPage>();
    REQUIRE(page.tracks.size() == 1);
    REQUIRE(page.tracks.front().playlist_set_video_id == "entry-three");
    REQUIRE(page.continuation->token == "last+/=");
    REQUIRE(page.continuation->in_body);

    auto& rows = data["onResponseReceivedActions"][1]["appendContinuationItemsAction"]["continuationItems"];
    rows.erase(rows.size() - 1);
    REQUIRE_FALSE(data.get<MusicPlaylistPage>().continuation);
    rows.clear();
    REQUIRE(data.get<MusicPlaylistPage>().tracks.empty());

    const auto original = fixture("playlist.json");
    json legacy = {{"continuationContents", {{"musicPlaylistShelfContinuation", original.at(shelf_path)}}}};
    legacy["continuationContents"]["musicPlaylistShelfContinuation"]["contents"].erase(2);
    REQUIRE(legacy.get<MusicPlaylistPage>().tracks.size() == 2);

    REQUIRE_THROWS_AS(json::object().get<MusicPlaylist>(), std::invalid_argument);
}

TEST_CASE("public playlists return typed details and track pages", "[youtube][playlist][live]") {
    if (!std::getenv("MUSIQ_LIVE_TESTS")) SKIP("set MUSIQ_LIVE_TESTS=1 to call public YouTube playlists");
    const YoutubeMusicSession session;
    const YoutubeMusicClient client(session);
    const YoutubeMusicPlaylists playlists(client);
    const MusicRequestOptions options{.auth = MusicAuth::Anonymous};
    const auto playlist = playlists.get("PLwNv9Hhd8gZjNoQdpd2kBa3fwXNeJjzDX", options);
    if (!playlist) FAIL(playlist.error().message);
    REQUIRE_FALSE(playlist->title.empty());
    REQUIRE_FALSE(playlist->tracks.empty());
    if (playlist->continuation) {
        const auto page = playlists.more(*playlist->continuation);
        if (!page) FAIL(page.error().message);
        REQUIRE_FALSE(page->tracks.empty());
    }
}
