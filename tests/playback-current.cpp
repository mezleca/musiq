#include "../src/playback/controller.hpp"
#include "../src/playback/mpv-player.hpp"

#include <catch2/catch_test_macros.hpp>
#include <utility>

TEST_CASE("playing the current video leaves playback and its queue untouched", "[playback][controller]") {
    MpvPlayer player(true);
    PlaybackController playback(player);
    MusicItem first;
    first.endpoint.video_id = "first";
    MusicItem current;
    current.endpoint.video_id = "current";
    current.playlist_set_video_id = "occurrence";
    playback.queue().replace({first, current, first}, 1, "playlist");
    const auto selected = playback.queue().current()->id;
    const auto first_id = playback.queue().entries().front().id;
    int changes = 0;
    playback.on_queue_change([&changes] { ++changes; });

    SECTION("playing") { player.pause(false); }
    SECTION("paused") { player.pause(true); }
    const bool paused = playback.paused();

    // search and home cards do not carry the playlist occurrence id
    MusicItem clicked;
    clicked.endpoint.video_id = "current";
    playback.play(std::move(clicked));

    CHECK(playback.queue().entries().size() == 3);
    CHECK(playback.queue().current()->id == selected);
    CHECK(playback.queue().entries().front().id == first_id);
    CHECK(playback.queue().position() == 1);
    CHECK(playback.queue().source() == "playlist");
    CHECK(playback.paused() == paused);
    CHECK_FALSE(playback.loading());
    CHECK(changes == 0);
}
