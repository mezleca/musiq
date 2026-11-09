#include "../src/youtube/schema/catalog.hpp"
#include "../src/youtube/schema/player.hpp"
#include "../src/youtube/client.hpp"
#include "../src/youtube/session.hpp"
#include "../src/youtube/catalog.hpp"
#include "../src/youtube/queue.hpp"
#include "fixtures.hpp"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <utility>

TEST_CASE("request context keeps client identity and visitor data together", "[youtube][request]") {
    const json web = MusicRequestContext{MusicClient::WebRemix, "visitor", "brand"};
    REQUIRE(web.at("client").at("clientName") == "WEB_REMIX");
    REQUIRE(web.at("client").at("visitorData") == "visitor");
    REQUIRE(web.at("user").at("onBehalfOfUser") == "brand");

    const json vision = MusicRequestContext{MusicClient::VisionOs, "visitor", "brand"};
    REQUIRE(vision.at("client").at("clientName") == "VISIONOS");
    REQUIRE_FALSE(vision.at("user").contains("onBehalfOfUser"));
}

TEST_CASE("player request only sends supplied playback fields", "[youtube][player]") {
    const json basic = MusicPlayerRequest{.video_id = "video"};
    REQUIRE(basic.at("videoId") == "video");
    REQUIRE(basic.at("contentCheckOk") == true);
    REQUIRE_FALSE(basic.contains("playlistId"));
    REQUIRE_FALSE(basic.contains("playbackContext"));
    REQUIRE_FALSE(basic.contains("serviceIntegrityDimensions"));

    const json full = MusicPlayerRequest{"video", "playlist", 12345, "proof"};
    REQUIRE(full.at("playlistId") == "playlist");
    REQUIRE(full.at(json::json_pointer("/playbackContext/contentPlaybackContext/signatureTimestamp")) == 12345);
    REQUIRE(full.at("serviceIntegrityDimensions").at("poToken") == "proof");
}

TEST_CASE("player parses direct URLs cipher formats and metadata", "[youtube][player]") {
    const auto player = fixture("player.json").get<MusicPlayerData>();
    REQUIRE(player.status == "OK");
    REQUIRE(player.video_details->length_seconds == 210);
    REQUIRE(player.video_details->thumbnails.size() == 1);
    REQUIRE(player.streaming_data->expires_in_seconds == 21540);
    const auto& formats = player.streaming_data->adaptive_formats;
    REQUIRE(formats.size() == 3);
    REQUIRE(formats[0].bitrate == 160000);
    REQUIRE(formats[0].content_length == 42000000);
    REQUIRE(formats[0].audio_track->is_default);
    REQUIRE(formats[1].bitrate == 128000);
    REQUIRE_FALSE(formats[1].signature_cipher.empty());
    REQUIRE(formats[1].url.empty());
    REQUIRE(formats[2].drm_families.at(0) == "WIDEVINE");
    REQUIRE(player.video_details->video_id == "public-video");
}

TEST_CASE("nonplayable player responses retain their reason", "[youtube][player]") {
    const auto player = json{{"playabilityStatus", {{"status", "LOGIN_REQUIRED"}, {"reason", "Sign in"}}}}.get<MusicPlayerData>();
    REQUIRE(player.status == "LOGIN_REQUIRED");
    REQUIRE(player.reason == "Sign in");
    REQUIRE_FALSE(player.streaming_data);
    REQUIRE_FALSE(player.video_details);
    REQUIRE_THROWS_AS(json::object().get<MusicPlayerData>(), json::exception);
}

TEST_CASE("numeric stream fields reject malformed values", "[youtube][player]") {
    REQUIRE(music_number("128000") == 128000);
    REQUIRE_THROWS_AS(music_number("128000garbage"), std::invalid_argument);
    REQUIRE_THROWS_AS(music_number("999999999999999999999999999"), std::invalid_argument);
    REQUIRE_THROWS_AS(music_number(1.5), json::exception);
    REQUIRE_THROWS_AS(music_number(true), json::exception);
}

TEST_CASE("visitor bootstrap reads the documented array section", "[youtube][player]") {
    const auto data = json::parse(R"([[null,null,["unrelated",123,"CgtSANITIZED"]]])").get<MusicVisitorData>();
    REQUIRE(data.visitor_data == "CgtSANITIZED");
    REQUIRE_THROWS_AS(json::parse(R"([[null,null,["missing"]]])").get<MusicVisitorData>(), std::invalid_argument);
}

TEST_CASE("catalog parses shelves endpoints artwork and continuations", "[youtube][catalog]") {
    const auto page = fixture("catalog.json").get<MusicPage>();
    REQUIRE(page.shelves.size() == 2);
    REQUIRE(page.shelves[0].title == "Albums");
    REQUIRE(page.shelves[0].items[0].endpoint.browse_id == "MPREexample");
    REQUIRE(page.shelves[0].items[0].endpoint.page_type == "MUSIC_PAGE_TYPE_ALBUM");
    REQUIRE(page.shelves[0].items[0].thumbnails.size() == 1);
    REQUIRE(page.shelves[1].items[0].endpoint.video_id == "public-video");
    REQUIRE(page.shelves[1].items[0].duration == "3:30");
    REQUIRE(page.shelves[1].items[0].playlist_set_video_id == "entry-1");
    REQUIRE(page.shelves[1].items[0].subtitle[0].endpoint.browse_id == "UCEXAMPLE");
    REQUIRE(page.shelves[1].continuation->token == "shelf+/=");
    REQUIRE(page.continuation->token == "page+/=");
    REQUIRE_THROWS_AS(json::object().get<MusicPage>(), std::invalid_argument);
}

TEST_CASE("search and browse continuations share parsed catalog items", "[youtube][catalog]") {
    auto data = fixture("catalog.json");
    auto& contents = data.at("contents");
    contents["tabbedSearchResultsRenderer"] = std::move(contents["singleColumnBrowseResultsRenderer"]);
    contents.erase("singleColumnBrowseResultsRenderer");
    REQUIRE(data.get<MusicPage>().shelves.size() == 2);

    const json::json_pointer shelf_path(
        "/contents/tabbedSearchResultsRenderer/tabs/0/tabRenderer/content/sectionListRenderer/contents/1/musicShelfRenderer"
    );
    const auto shelf = data.at(shelf_path);
    const auto page = json{{"continuationContents", {{"musicShelfContinuation", shelf}}}}.get<MusicPage>();
    REQUIRE(page.shelves[0].items[0].title == "Example song");
    REQUIRE(page.continuation->token == "shelf+/=");
}

TEST_CASE("suggestions preserve server queries and distinguish history", "[youtube][catalog]") {
    const auto suggestions = fixture("suggestions.json").get<MusicSuggestions>();
    REQUIRE(suggestions.queries.size() == 2);
    REQUIRE(suggestions.queries[0].query == "Example song");
    REQUIRE_FALSE(suggestions.queries[0].history);
    REQUIRE(suggestions.queries[1].history);
}

TEST_CASE("search reads cards and item sections returned by the public endpoint", "[youtube][catalog]") {
    const auto page = fixture("search.json").get<MusicPage>();
    REQUIRE(page.shelves.size() == 2);
    REQUIRE(page.shelves[0].items[0].endpoint.video_id == "top-video");
    REQUIRE(page.shelves[0].items[0].thumbnails.size() == 1);
    REQUIRE(page.shelves[1].items[0].endpoint.video_id == "row-video");
    REQUIRE(page.shelves[1].items[0].title == "Example song");
}

TEST_CASE("next parses primary audio rows without adding video counterparts", "[youtube][next]") {
    const auto page = fixture("next.json").get<MusicNextPage>();
    REQUIRE(page.items.size() == 1);
    REQUIRE(page.items[0].endpoint.video_id == "audio-id");
    REQUIRE(page.lyrics.browse_id == "MPLYexample");
    REQUIRE(page.related.browse_id == "MPTRexample");
    REQUIRE(page.continuation->token == "radio-token");
}

TEST_CASE("single tracks accept empty artist columns and plain text subtitles", "[youtube][catalog]") {
    auto data = fixture("single.json");
    const json::json_pointer artist_path(
        "/contents/twoColumnBrowseResultsRenderer/secondaryContents/sectionListRenderer/contents/0/musicShelfRenderer/"
        "contents/0/musicResponsiveListItemRenderer/flexColumns/1/musicResponsiveListItemFlexColumnRenderer/text"
    );
    const auto page = data.get<MusicPage>();
    REQUIRE(page.shelves.size() == 1);
    REQUIRE(page.shelves[0].items.size() == 1);
    const auto& track = page.shelves[0].items[0];
    REQUIRE(track.title == "Example single");
    REQUIRE(track.endpoint.video_id == "single-track");
    REQUIRE(track.duration == "3:22");
    REQUIRE(track.subtitle.empty());

    data.at(artist_path) = {{"simpleText", "Example artist"}};
    const auto plain = data.get<MusicPage>();
    REQUIRE(plain.shelves[0].items[0].subtitle.size() == 1);
    REQUIRE(plain.shelves[0].items[0].subtitle[0].text == "Example artist");
    REQUIRE(music_text(json::object()).empty());
}

TEST_CASE("next continuation reads the playlist panel directly", "[youtube][next]") {
    const auto data = fixture("next.json");
    const json::json_pointer panel_path(
        "/contents/singleColumnMusicWatchNextResultsRenderer/tabbedRenderer/watchNextTabbedResultsRenderer/tabs/0/"
        "tabRenderer/content/musicQueueRenderer/content/playlistPanelRenderer"
    );
    const auto& panel = data.at(panel_path);
    const auto page = json{{"continuationContents", {{"playlistPanelContinuation", panel}}}}.get<MusicNextPage>();
    REQUIRE(page.items.size() == 1);
    REQUIRE(page.continuation->token == "radio-token");
    const json radio = MusicNextRequest{.playlist_id = "RDexample"};
    REQUIRE_FALSE(radio.contains("videoId"));
    REQUIRE(radio.at("playlistId") == "RDexample");
}

TEST_CASE("next skips unavailable rows and accepts missing track metadata", "[youtube][next]") {
    const json row = {{"title", {{"simpleText", "Song"}}}, {"videoId", "video"}};
    const json panel = {
        {"contents",
         {{{"playlistPanelVideoRenderer", row}},
          {{"playlistPanelVideoRenderer", {{"unplayableText", {{"simpleText", "Unavailable"}}}}}}}}
    };
    const auto page = json{{"continuationContents", {{"playlistPanelContinuation", panel}}}}.get<MusicNextPage>();
    REQUIRE(page.items.size() == 1);
    REQUIRE(page.items.front().endpoint.video_id == "video");
    REQUIRE(page.items.front().duration.empty());
    REQUIRE(page.items.front().subtitle.empty());
    REQUIRE(page.items.front().thumbnails.empty());

    const json request = MusicNextRequest{.video_id = "video"};
    REQUIRE(request.at("playlistId") == "RDAMVMvideo");
}

TEST_CASE("account and continuation guards fail before sending requests", "[youtube][request]") {
    const YoutubeMusicSession session;
    const YoutubeMusicClient client(session);
    const YoutubeMusicCatalog catalog(client);
    const YoutubeMusicQueue queue(client);
    const auto account = client.account_menu();
    REQUIRE_FALSE(account);
    REQUIRE(account.error().kind == YoutubeErrorKind::Authentication);
    const auto player = client.player({.video_id = "video"}, {.client = MusicClient::VisionOs, .auth = MusicAuth::Required});
    REQUIRE_FALSE(player);
    REQUIRE(player.error().kind == YoutubeErrorKind::Authentication);
    const auto page = catalog.more({.token = "opaque", .auth = MusicAuth::Required, .identity = "another-channel"});
    REQUIRE_FALSE(page);
    REQUIRE(page.error().kind == YoutubeErrorKind::Authentication);
    REQUIRE_FALSE(queue.more({.token = "browse-token"}));
    REQUIRE(client.request_options({}).auth == MusicAuth::Anonymous);
}

TEST_CASE("pagination keeps its token and resolves scope without account leakage", "[youtube][request]") {
    auto page = fixture("catalog.json").get<MusicPage>();
    page.set_scope(MusicRoute::Search, {MusicClient::WebRemix, MusicAuth::Anonymous, "visitor"}, "private-channel");
    REQUIRE(page.continuation->token == "page+/=");
    REQUIRE(page.continuation->route == MusicRoute::Search);
    REQUIRE(page.continuation->identity.empty());
    REQUIRE(page.continuation->visitor_data == "visitor");
    REQUIRE(page.shelves[1].continuation->token == "shelf+/=");
    REQUIRE(page.shelves[1].continuation->route == MusicRoute::Search);
}

TEST_CASE("schema conversion preserves the actual response status on failure", "[youtube][request]") {
    const auto result = youtube_parse<MusicPlayerData>(YoutubeResponse{json::object(), 206});
    REQUIRE_FALSE(result);
    REQUIRE(result.error().kind == YoutubeErrorKind::Parse);
    REQUIRE(result.error().status == 206);
}
