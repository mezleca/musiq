#include "../src/playback/queue.hpp"

#include <catch2/catch_test_macros.hpp>

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
