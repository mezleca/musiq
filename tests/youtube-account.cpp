#include "../src/youtube/schema/account.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>

static json fixture(const char* name) {
    std::ifstream input(std::filesystem::path(MUSIQ_SOURCE_DIR) / "tests" / "fixtures" / name);
    REQUIRE(input.is_open());
    return json::parse(input);
}

TEST_CASE("account menu parses profile and selected identity", "[youtube][account]") {
    const auto account = fixture("account-menu.json").get<MusicAccountMenu>();
    REQUIRE(account.name == "Brand channel");
    REQUIRE(account.handle == "@brand");
    REQUIRE(account.avatar_url == "https://example.invalid/large.jpg");
    REQUIRE(account.data_sync_id == "brand");
    const json expected = {
        {"name", "Brand channel"},
        {"handle", "@brand"},
        {"avatar_url", "https://example.invalid/large.jpg"},
        {"data_sync_id", "brand"}
    };
    REQUIRE(json(account) == expected);
}

TEST_CASE("account menu permits missing optional metadata", "[youtube][account]") {
    auto data = fixture("account-menu.json");
    const json::json_pointer path("/actions/0/openPopupAction/popup/multiPageMenuRenderer/header/activeAccountHeaderRenderer");
    auto& header = data.at(path);
    header.erase("channelHandle");
    header.erase("accountPhoto");
    data.erase("responseContext");

    const auto account = data.get<MusicAccountMenu>();
    REQUIRE(account.name == "Brand channel");
    REQUIRE(account.handle.empty());
    REQUIRE(account.avatar_url.empty());
    REQUIRE(account.data_sync_id.empty());
}

TEST_CASE("account menu reads its header separately from menu sections", "[youtube][account]") {
    auto data = fixture("account-menu.json");
    auto& menu = data.at(json::json_pointer("/actions/0/openPopupAction/popup/multiPageMenuRenderer"));
    menu["sections"] = {{{"activeAccountHeaderRenderer", {{"accountName", {{"simpleText", "Other channel"}}}}}}};
    REQUIRE(data.get<MusicAccountMenu>().name == "Brand channel");

    menu.erase("sections");
    REQUIRE(data.get<MusicAccountMenu>().name == "Brand channel");
}

TEST_CASE("malformed account responses fail parsing", "[youtube][account]") {
    REQUIRE_THROWS_AS(json::object().get<MusicAccountMenu>(), json::exception);
    REQUIRE_THROWS_AS(json({{"actions", json::array()}}).get<MusicAccountMenu>(), std::invalid_argument);
    REQUIRE_THROWS_AS(fixture("account-menu.json").get<MusicAccountsList>(), std::invalid_argument);

    auto data = fixture("account-menu.json");
    const json::json_pointer path("/actions/0/openPopupAction/popup/multiPageMenuRenderer/header/activeAccountHeaderRenderer");
    auto& header = data.at(path);
    SECTION("missing name") {
        header.erase("accountName");
        REQUIRE_THROWS_AS(data.get<MusicAccountMenu>(), json::exception);
    }
    SECTION("wrong field type") {
        header["accountName"] = {{"simpleText", 42}};
        REQUIRE_THROWS_AS(data.get<MusicAccountMenu>(), json::exception);
    }
    SECTION("empty name") {
        header["accountName"] = {{"simpleText", ""}};
        REQUIRE_THROWS_AS(data.get<MusicAccountMenu>(), std::invalid_argument);
    }
}

TEST_CASE("account list preserves selectable channels and token precedence", "[youtube][account]") {
    const auto accounts = fixture("accounts-list.json").get<MusicAccountsList>();
    REQUIRE(accounts.identities.size() == 3);
    REQUIRE(accounts.identities[0].name == "Primary");
    REQUIRE(accounts.identities[0].data_sync_id == "selected");
    REQUIRE(accounts.identities[0].handle.empty());
    REQUIRE(accounts.identities[1].data_sync_id == "brand");
    REQUIRE(accounts.identities[2].data_sync_id == "page-only");
    const json expected = {{"name", "Brand"}, {"handle", "@brand"}, {"data_sync_id", "brand"}};
    REQUIRE(json(accounts).at("identities")[1] == expected);
}

TEST_CASE("account list deduplicates identities", "[youtube][account]") {
    auto data = fixture("accounts-list.json");
    const json::json_pointer path(
        "/actions/0/getMultiPageMenuAction/menu/multiPageMenuRenderer/sections/0/"
        "accountSectionListRenderer/contents/0/accountItemSectionRenderer/contents"
    );
    auto& entries = data.at(path);
    entries.push_back(entries.at(1));
    const auto accounts = data.get<MusicAccountsList>();
    REQUIRE(accounts.identities.size() == 3);
    REQUIRE(accounts.identities[1].data_sync_id == "brand");
}
