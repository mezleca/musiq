#include "../src/playback/queue.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("queue repeat modes distinguish once, track and playlist", "[playback]") {
    const MusicItem song{.endpoint = {.video_id = "video"}};
    PlaybackQueue queue;
    queue.replace({song, song});

    SECTION("once is consumed at the end of the current track") {
        queue.set_repeat(true);
        REQUIRE(queue.loop() == LoopMode::Once);
        REQUIRE(queue.take_repeat());
        REQUIRE_FALSE(queue.take_repeat());
        REQUIRE(queue.loop() == LoopMode::None);
    }
    SECTION("track repeat survives repeated ends and manual navigation") {
        queue.set_loop(LoopMode::Track);
        REQUIRE(queue.take_repeat());
        REQUIRE(queue.take_repeat());
        REQUIRE(queue.advance());
        REQUIRE(queue.take_repeat());
        queue.set_loop(LoopMode::None);
        REQUIRE_FALSE(queue.take_repeat());
    }
    SECTION("playlist repeat wraps to the first entry") {
        const auto first = queue.current()->id;
        queue.set_loop(LoopMode::Playlist);
        REQUIRE_FALSE(queue.take_repeat());
        REQUIRE(queue.advance());
        REQUIRE(queue.can_advance());
        REQUIRE(queue.advance());
        REQUIRE(queue.current()->id == first);
    }
    SECTION("one-entry playlists repeat even when shuffle is enabled") {
        queue.replace({song});
        queue.set_loop(LoopMode::Playlist);
        queue.set_random(true);
        REQUIRE(queue.take_repeat());
        REQUIRE(queue.advance());
        REQUIRE(queue.position() == 0);
    }
    SECTION("replacing the queue preserves continuous repeat") {
        queue.set_loop(LoopMode::Track);
        queue.replace({song});
        REQUIRE(queue.loop() == LoopMode::Track);
    }
    SECTION("selecting a track preserves a previously chosen once mode") {
        queue.set_loop(LoopMode::Once);
        queue.replace({song});
        REQUIRE(queue.loop() == LoopMode::Once);
        REQUIRE(queue.take_repeat());
        REQUIRE(queue.loop() == LoopMode::None);
    }
    SECTION("manual navigation preserves once until it is consumed") {
        queue.set_loop(LoopMode::Once);
        queue.advance();
        REQUIRE(queue.loop() == LoopMode::Once);
        queue.previous();
        REQUIRE(queue.loop() == LoopMode::Once);
        queue.select(queue.entries()[1].id);
        REQUIRE(queue.loop() == LoopMode::Once);
        REQUIRE(queue.take_repeat());
        REQUIRE_FALSE(queue.take_repeat());
    }
}

TEST_CASE("queue edits preserve the selected occurrence of repeated tracks", "[playback]") {
    const MusicItem song{.title = "Repeated song", .endpoint = {.video_id = "same-video"}};
    PlaybackQueue queue;
    queue.replace({song, song, song}, 1);
    const auto first = queue.entries()[0].id;
    const auto selected = queue.current()->id;
    const auto last = queue.entries()[2].id;

    queue.move(first, 2);
    REQUIRE(queue.current()->id == selected);
    queue.play_next(song);
    const auto inserted = queue.entries()[queue.position() + 1].id;
    REQUIRE(inserted != selected);
    queue.remove(last);
    REQUIRE(queue.current()->id == selected);
    queue.remove(selected);
    REQUIRE(queue.current()->id == inserted);

    queue.move(inserted, queue.entries().size() - 1);
    REQUIRE(queue.current()->id == inserted);
    queue.remove(inserted);
    REQUIRE(queue.current()->id == first);

    queue.replace({song});
    REQUIRE_FALSE(queue.remove(first));
    REQUIRE(queue.current()->id != first);
}

TEST_CASE("queue source survives navigation and pagination until the list is edited", "[playback]") {
    const MusicItem song{.endpoint = {.video_id = "video"}};
    PlaybackQueue queue;
    queue.replace({song, song}, 0, "album");
    queue.advance();
    queue.previous();
    queue.select(queue.entries()[1].id);
    queue.append(song, true);
    REQUIRE(queue.source() == "album");

    SECTION("manual append") { queue.append(song); }
    SECTION("play next") { queue.play_next(song); }
    SECTION("remove") { queue.remove(queue.entries()[0].id); }
    SECTION("reorder") { queue.move(queue.entries()[0].id, 2); }
    SECTION("replace") { queue.replace({song}); }
    REQUIRE(queue.source().empty());
}

TEST_CASE("queue removal keeps the cursor on the selected track or its nearest successor", "[playback]") {
    const MusicItem song{.endpoint = {.video_id = "video"}};
    PlaybackQueue queue;
    queue.replace({song, song, song}, 1);
    const auto first = queue.entries()[0].id;
    const auto selected = queue.current()->id;
    const auto last = queue.entries()[2].id;

    SECTION("removing before the selection") {
        queue.remove(first);
        REQUIRE(queue.current()->id == selected);
        REQUIRE(queue.position() == 0);
    }
    SECTION("removing after the selection") {
        queue.remove(last);
        REQUIRE(queue.current()->id == selected);
    }
    SECTION("removing the selected track") {
        queue.remove(selected);
        REQUIRE(queue.current()->id == last);
    }
    SECTION("removing the selected tail and the final entry") {
        queue.select(last);
        queue.remove(last);
        REQUIRE(queue.current()->id == selected);
        queue.remove(selected);
        REQUIRE(queue.current()->id == first);
        queue.remove(first);
        REQUIRE(queue.current() == nullptr);
        REQUIRE(queue.position() == 0);
    }
}

TEST_CASE("queue reordering preserves entry ids and the selected occurrence", "[playback]") {
    const MusicItem song{.endpoint = {.video_id = "same-video"}};
    PlaybackQueue queue;
    queue.replace({song, song, song}, 1);
    const auto first = queue.entries()[0].id;
    const auto selected = queue.current()->id;
    const auto last = queue.entries()[2].id;

    SECTION("move an entry backward across the selection") {
        REQUIRE(queue.move(last, 0));
        REQUIRE(queue.entries()[0].id == last);
        REQUIRE(queue.entries()[1].id == first);
        REQUIRE(queue.position() == 2);
    }
    SECTION("move the selected entry forward and clamp the destination") {
        REQUIRE(queue.move(selected, 100));
        REQUIRE(queue.entries()[0].id == first);
        REQUIRE(queue.entries()[1].id == last);
        REQUIRE(queue.position() == 2);
    }
    SECTION("ignore an unchanged position or missing entry") {
        const auto revision = queue.revision();
        REQUIRE_FALSE(queue.move(selected, 1));
        REQUIRE_FALSE(queue.move(0, 0));
        REQUIRE(queue.revision() == revision);
    }
    REQUIRE(queue.current()->id == selected);
    REQUIRE(queue.entries().size() == 3);
}
