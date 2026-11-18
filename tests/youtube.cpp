#include "../src/youtube/catalog.hpp"
#include "../src/youtube/client.hpp"
#include "../src/youtube/playlists.hpp"
#include "../src/youtube/queue.hpp"
#include "../src/youtube/schema/player.hpp"
#include "../src/youtube/session.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>

static YoutubeMusicConfig test_config() {
    const char* api_url = std::getenv("MUSIQ_TEST_API_URL");
    const char* web_url = std::getenv("MUSIQ_TEST_WEB_URL");
    if (api_url == nullptr || web_url == nullptr) {
        SKIP("run through tests/python/server.py to start the local server");
    }
    return {.api_url = api_url, .web_url = web_url};
}

static YoutubeMusicSession authenticated_session(const YoutubeMusicConfig& config) {
    YoutubeMusicSession session;
    REQUIRE(session.authenticate("SAPISID=local-secret", "local-sync-id", config));
    return session;
}

TEST_CASE("anonymous discovery flow", "[youtube][endpoint]") {
    const YoutubeMusicSession session;
    const auto config = test_config();
    const YoutubeMusicClient client(session, config);
    const auto visitor = client.visitor_data();
    REQUIRE(visitor);
    REQUIRE(visitor->visitor_data == "CgtlocalVisitor");

    const YoutubeMusicCatalog catalog(client);
    const auto suggestions = YoutubeMusicCatalog(client).suggestions("seed query");
    REQUIRE(suggestions);
    REQUIRE(suggestions->queries.size() == 1);
    REQUIRE(suggestions->queries.front().query == "la vie en rose karaoke");

    const auto results = catalog.search({.query = "search query"});
    REQUIRE(results);
    REQUIRE(results->shelves.empty());

    const auto home = catalog.browse({.browse_id = "FEmusic_home"});
    REQUIRE(home);
    REQUIRE(home->shelves.empty());
    REQUIRE(home->continuation);
    const auto more_home = catalog.more(*home->continuation);
    REQUIRE(more_home);
}

TEST_CASE("playback and queue continuation flow", "[youtube][endpoint]") {
    const YoutubeMusicSession session;
    const auto config = test_config();
    const YoutubeMusicClient client(session, config);
    const auto player = client.player({.video_id = "video-id"});
    REQUIRE(player);
    REQUIRE(player->status == "LOGIN_REQUIRED");
    REQUIRE(player->reason == "Sign in");

    const auto queue = YoutubeMusicQueue(client).next({.video_id = "video-id"});
    REQUIRE(queue);
    REQUIRE(queue->items.empty());
    REQUIRE(queue->continuation);
    const auto more_queue = YoutubeMusicQueue(client).more(*queue->continuation);
    REQUIRE(more_queue);
}

TEST_CASE("authenticated account selection flow", "[youtube][endpoint]") {
    const auto config = test_config();
    const auto session = authenticated_session(config);
    REQUIRE(session.identity() == "local-sync-id");
    REQUIRE(session.user());
    REQUIRE(session.user()->name == "Local User");
    REQUIRE(session.identities().size() == 1);
}

TEST_CASE("authenticated playlist read and continuation flow", "[youtube][endpoint]") {
    const auto config = test_config();
    const auto session = authenticated_session(config);
    const YoutubeMusicClient client(session, config);
    const YoutubeMusicPlaylists playlists(client);
    const auto library = playlists.library();
    REQUIRE(library);

    const auto playlist = playlists.get("PLlocal-created");
    REQUIRE(playlist);
    REQUIRE(playlist->title == "Local playlist");
    REQUIRE(playlist->continuation);
    const auto more_tracks = playlists.more(*playlist->continuation);
    REQUIRE(more_tracks);
}

TEST_CASE("authenticated playlist write flow", "[youtube][endpoint]") {
    const auto config = test_config();
    const auto session = authenticated_session(config);
    const YoutubeMusicClient client(session, config);
    const YoutubeMusicPlaylists playlists(client);

    const auto created = playlists.create({.title = "Local playlist"});
    REQUIRE(created);
    REQUIRE(created->id == "PLlocal-created");
    const auto added = playlists.add({.playlist_id = "PLlocal-created", .video_ids = {"video-id"}});
    REQUIRE(added);

    const auto image = std::filesystem::temp_directory_path() / "musiq-endpoint-cover.png";
    {
        std::ofstream file(image, std::ios::binary);
        file << "local-image";
    }
    const auto updated = playlists.update(
        {.playlist_id = "PLlocal-created",
         .title = "Renamed playlist",
         .description = "Local description",
         .privacy = MusicPlaylistPrivacy::Public,
         .thumbnail_path = image}
    );
    std::filesystem::remove(image);
    REQUIRE(updated);

    const auto removed = playlists.remove(
        {.playlist_id = "PLlocal-created", .entries = {{.video_id = "video-id", .set_video_id = "set-video-id"}}}
    );
    REQUIRE(removed);
}
