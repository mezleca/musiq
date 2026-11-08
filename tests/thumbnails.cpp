#include "../src/app/resources/thumbnails.hpp"
#include "../src/utils/media.hpp"
#include "../src/youtube/schema/common.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace media;

TEST_CASE("covers choose the smallest variant that fills their physical size", "[thumbnails]") {
    const std::vector<MusicThumbnail> sources{{"large", 544, 544}, {"small", 60, 60}, {"medium", 120, 120}, {"card", 226, 226}};

    REQUIRE(select_thumbnail(sources, 48.0f)->url == "small");
    REQUIRE(select_thumbnail(sources, 96.0f)->url == "medium");
    REQUIRE(select_thumbnail(sources, 120.0f)->url == "medium");
    REQUIRE(select_thumbnail(sources, 176.0f)->url == "card");
    REQUIRE(select_thumbnail(sources, 352.0f)->url == "large");
    REQUIRE(select_thumbnail(sources, 1000.0f)->url == "large");

    // cropping a rectangle uses its shorter side
    const std::vector<MusicThumbnail> rectangular{{"wide", 320, 90}, {"square", 120, 120}};
    REQUIRE(select_thumbnail(rectangular, 100.0f)->url == "square");
    REQUIRE(select_thumbnail(rectangular, 400.0f)->url == "square");
}

TEST_CASE("cover selection handles empty urls and missing dimensions", "[thumbnails]") {
    REQUIRE(select_thumbnail({}, 48.0f) == nullptr);
    const std::vector<MusicThumbnail> empty{{"", 120, 120}};
    REQUIRE(select_thumbnail(empty, 48.0f) == nullptr);

    const std::vector<MusicThumbnail> unknown{{"unknown", 0, 0}, {"known", 60, 60}};
    REQUIRE(select_thumbnail(unknown, 48.0f)->url == "known");
    REQUIRE(select_thumbnail(std::span(unknown).first(1), 48.0f)->url == "unknown");
}
