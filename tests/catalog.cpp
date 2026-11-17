#include "../src/app/resources/thumbnails.hpp"
#include "../src/app/widgets/catalog-card.hpp"
#include "../src/app/widgets/catalog-results.hpp"
#include "../src/app/widgets/track-list.hpp"
#include "../src/utils/thread-pool.hpp"
#include "../src/youtube/catalog-browser.hpp"
#include "../src/youtube/schema/common.hpp"

#include <arbor/backends/backend.hpp>
#include <arbor/imgui/context-scope.hpp>
#include <arbor/input/router.hpp>
#include <arbor/runtime.hpp>
#include <arbor/surface.hpp>

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class TrackListTestBackend final : public ui::Backend {
public:
    bool initialize() override {
        return true;
    }

    bool initialize_imgui() override {
        return true;
    }

    void process_events(ui::Surface&) override {}
    void shutdown_imgui() override {}
    void begin_frame(ui::Color) override {}
    void set_mouse_cursor(ImGuiMouseCursor) override {}

    std::optional<double> render(ImDrawData*, bool) override {
        return std::nullopt;
    }

    float content_scale() const override {
        return 1.0f;
    }

    std::uint64_t window_id() const override {
        return 1;
    }

    ImVec2 display_size() const override {
        return {640, 480};
    }
};

struct TrackListFixture {
    TrackListFixture() {
        ImGui::GetIO().DisplaySize = {640, 480};
        unsigned char* pixels;
        int width, height;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }

    void update(int frames = 1) {
        for (int frame = 0; frame < frames; ++frame) {
            surface.begin_frame();
            surface.update(1.0f / 60.0f);
            surface.end_frame();
        }
    }

    ThumbnailCache thumbnails;
    ui::Runtime runtime;
    ui::Surface surface{runtime, {.backend = std::make_unique<TrackListTestBackend>()}};
    ui::ImGuiContextScope scope{surface.imgui_context()};
};

TEST_CASE_METHOD(
    TrackListFixture, "track rows retire outside the viewport after focus and capture are released", "[tracks][regression]"
) {
    auto& list = surface.root().add<TrackList>();
    auto& card = list.add_track(1, MusicItem{}, thumbnails);
    SECTION("focus pins the row") {
        REQUIRE(surface.input_router().set_focus(&card));
    }
    SECTION("pointer capture pins the row") {
        REQUIRE(surface.input_router().capture_pointer(card));
    }
    update(4);
    REQUIRE(list.find_track(1) == &card);
    surface.input_router().set_focus(nullptr);
    surface.input_router().release_pointer();
    update(4);
    REQUIRE(list.find_track(1) == nullptr);
    REQUIRE(list.children().empty());
}

TEST_CASE_METHOD(
    TrackListFixture, "hidden track pages preserve rows until visible updates stop requesting them", "[tracks][regression]"
) {
    auto& list = surface.root().add<TrackList>();
    auto* card = &list.add_track(1, MusicItem{}, thumbnails);
    list.set_visible(false);
    update(10);
    list.set_visible(true);
    surface.begin_frame();
    surface.update(1.0f / 60.0f);
    REQUIRE(list.find_track(1) == card);
    surface.end_frame();
}

TEST_CASE_METHOD(TrackListFixture, "loading cards reserve the same geometry as populated cards", "[tracks][loading]") {
    const MusicItem item{.title = "A track", .endpoint = {.video_id = "video"}};
    auto& loading = surface.root().add<MediaCardWidget>(MusicItem{}, thumbnails, true);
    auto& loaded = surface.root().add<MediaCardWidget>(item, thumbnails);
    loading.set_size({ui::grow(), ui::px(64)});
    loaded.set_size({ui::grow(), ui::px(64)});
    for (float scale : {1.0f, 1.25f, 2.0f}) {
        surface.set_scale(scale);
        for (int frame = 0; frame < 3; ++frame) {
            surface.frame();
        }
        REQUIRE_FALSE(loading.enabled());
        REQUIRE(loading.layout().size().x == loaded.layout().size().x);
        REQUIRE(loading.layout().size().y == loaded.layout().size().y);
        REQUIRE(loading.children().front()->layout().size().x == loaded.children().front()->layout().size().x);
        REQUIRE(loading.children().front()->layout().size().y == loaded.children().front()->layout().size().y);
        const auto* loading_details = loading.find("details");
        const auto* loaded_details = loaded.find("details");
        REQUIRE(loading_details->layout().size().y == loaded_details->layout().size().y);
    }
}

TEST_CASE("catalog detects in-place pagination without a separate append event", "[tracks][catalog]") {
    ThumbnailCache thumbnails;
    CatalogResultsWidget widget("catalog", "Results", thumbnails);
    CatalogPageState state;
    state.page = std::make_shared<MusicPage>();
    MusicShelf shelf;
    shelf.items.push_back(MusicItem{.endpoint = {.video_id = "one"}});
    state.page->shelves.push_back(shelf);
    widget.show(state);
    auto* rows = widget.find("results");
    REQUIRE(rows->children().size() == 1);
    state.page->shelves.push_back(shelf);
    widget.show(state);
    REQUIRE(rows->children().size() == 2);
    widget.show(state);
    REQUIRE(rows->children().size() == 2);
}

TEST_CASE("catalog preserves consecutive playlist and track groups", "[tracks][catalog]") {
    std::vector<bool> playlists;
    std::vector<std::pair<std::string, std::size_t>> expected;
    SECTION("empty shelf") {}
    SECTION("single track") {
        playlists = {false};
        expected = {{"tracks", 1}};
    }
    SECTION("single playlist") {
        playlists = {true};
        expected = {{"playlists", 1}};
    }
    SECTION("mixed shelf") {
        playlists = {true, true, false, false, true, false};
        expected = {{"playlists", 2}, {"tracks", 2}, {"playlists", 1}, {"tracks", 1}};
    }

    ThumbnailCache thumbnails;
    CatalogResultsWidget widget("catalog", "Results", thumbnails);
    CatalogPageState state;
    state.page = std::make_shared<MusicPage>();
    MusicShelf shelf;
    for (const bool playlist : playlists) {
        shelf.items.push_back(
            MusicItem{.endpoint = {.video_id = playlist ? "" : "video", .browse_id = playlist ? "VLplaylist" : ""}}
        );
    }
    state.page->shelves.push_back(std::move(shelf));
    widget.show(state);

    const auto* rows = widget.find("results");
    REQUIRE(rows != nullptr);
    if (playlists.empty()) {
        REQUIRE(rows->children().empty());
        return;
    }

    REQUIRE(rows->children().size() == 1);
    std::vector<std::pair<std::string, std::size_t>> groups;
    for (const auto& group : rows->children().front()->children()) {
        const auto* tracks = dynamic_cast<const TrackList*>(group.get());
        groups.emplace_back(group->id(), tracks ? tracks->item_count() : group->children().size());
    }
    REQUIRE(groups == expected);
}
