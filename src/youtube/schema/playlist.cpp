#include "playlist.hpp"

#include <charconv>
#include <stdexcept>
#include <utility>

void to_json(json& data, const MusicPlaylistRequest& value) {
    data = {{"browseId", value.playlist_id.starts_with("VL") ? value.playlist_id : "VL" + value.playlist_id}};
}

static const json& playlist_shelf(const json& data) {
    const json::json_pointer continuation("/continuationContents/musicPlaylistShelfContinuation");
    if (data.contains(continuation)) return data.at(continuation);

    static const auto paths = {
        json::json_pointer("/contents/twoColumnBrowseResultsRenderer/secondaryContents/sectionListRenderer/contents"),
        json::json_pointer("/contents/singleColumnBrowseResultsRenderer/tabs/0/tabRenderer/content/sectionListRenderer/contents")
    };
    for (const auto& path : paths) {
        if (!data.contains(path)) continue;
        for (const auto& section : data.at(path)) {
            const auto shelf = section.find("musicPlaylistShelfRenderer");
            if (shelf != section.end()) return *shelf;
        }
    }

    throw std::invalid_argument("missing playlist tracks");
}

// newer playlist pages put the next token in the last row instead of the shelf's continuations field
static std::optional<MusicContinuation> playlist_continuation(const json& rows) {
    if (rows.empty()) return std::nullopt;
    const json::json_pointer endpoint("/continuationItemRenderer/continuationEndpoint");
    const auto& last = rows.back();
    if (!last.contains(endpoint)) return std::nullopt;
    const auto& next = last.at(endpoint);
    MusicContinuation continuation;
    continuation.in_body = true;

    const auto command = next.find("continuationCommand");
    if (command != next.end()) {
        command->at("token").get_to(continuation.token);
        return continuation;
    }

    const auto executor = next.find("commandExecutorCommand");
    if (executor != next.end()) {
        for (const auto& entry : executor->at("commands")) {
            const auto item = entry.find("continuationCommand");
            if (item != entry.end() && item->value("request", std::string{}) == "CONTINUATION_REQUEST_TYPE_BROWSE") {
                item->at("token").get_to(continuation.token);
                return continuation;
            }
        }
    }

    return std::nullopt;
}

static MusicPlaylistPage playlist_rows(const json& rows) {
    MusicPlaylistPage page;
    for (const auto& row : rows) {
        const auto item = row.find("musicResponsiveListItemRenderer");
        if (item != row.end()) page.tracks.push_back(item->get<MusicItem>());
    }

    page.continuation = playlist_continuation(rows);
    return page;
}

static MusicPlaylistPage playlist_tracks(const json& shelf) {
    const auto rows = shelf.find("contents");
    auto page = rows == shelf.end() ? MusicPlaylistPage{} : playlist_rows(*rows);
    if (!page.continuation) page.continuation = music_continuation(shelf);
    return page;
}

void from_json(const json& data, MusicPlaylistPage& value) {
    const auto actions = data.find("onResponseReceivedActions");
    if (actions != data.end()) {
        for (const auto& action : *actions) {
            const auto append = action.find("appendContinuationItemsAction");
            if (append != action.end()) {
                value = playlist_rows(append->at("continuationItems"));
                return;
            }
        }
    }

    value = playlist_tracks(playlist_shelf(data));
}

void to_json(json& data, const MusicPlaylistPage& value) {
    data = {{"tracks", value.tracks}};
    if (value.continuation) data["continuation"] = *value.continuation;
}

static const json* playlist_header(const json& data, MusicPlaylist& playlist) {
    static const auto paths = {
        json::json_pointer("/header"),
        json::json_pointer("/contents/twoColumnBrowseResultsRenderer/tabs/0/tabRenderer/content/sectionListRenderer/contents/0"),
        json::json_pointer(
            "/contents/singleColumnBrowseResultsRenderer/tabs/0/tabRenderer/content/sectionListRenderer/contents/0"
        )
    };

    for (const auto& path : paths) {
        if (!data.contains(path)) continue;
        const auto& entry = data.at(path);
        const auto editable = entry.find("musicEditablePlaylistDetailHeaderRenderer");
        const auto& header = editable == entry.end() ? entry : editable->at("header");
        if (editable != entry.end()) {
            playlist.owned = true;
            playlist.privacy = editable->at("editHeader").at("musicPlaylistEditHeaderRenderer").at("privacy").get<std::string>();
        }

        for (const auto key : {"musicResponsiveHeaderRenderer", "musicDetailHeaderRenderer"}) {
            const auto renderer = header.find(key);
            if (renderer != header.end()) return &*renderer;
        }
    }

    return nullptr;
}

static std::optional<std::int64_t> playlist_count(std::string_view text) {
    text = text.substr(0, text.find(' '));
    std::string digits;
    for (const auto digit : text) {
        if (digit != ',') digits += digit;
    }

    std::int64_t count = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), count);
    if (error != std::errc{} || end != digits.data() + digits.size()) return std::nullopt;
    return count;
}

static void playlist_author(const json& header, MusicPlaylist& playlist) {
    const auto strapline = header.find("straplineTextOne");
    if (strapline != header.end()) {
        const auto& runs = strapline->at("runs");
        if (!runs.empty()) playlist.author = runs.front().get<MusicTextRun>();
        return;
    }

    const auto subtitle = header.find("subtitle");
    if (subtitle != header.end()) {
        for (const auto& run : subtitle->at("runs")) {
            auto text = run.get<MusicTextRun>();
            if (text.endpoint.browse_id.starts_with("UC")) {
                playlist.author = std::move(text);
                return;
            }
        }
    }

    const json::json_pointer facepile("/facepile/avatarStackViewModel");
    if (!header.contains(facepile)) return;

    // collaboration playlists expose their author only through the facepile context
    const auto& avatars = header.at(facepile);
    const auto& context = avatars.at("rendererContext");

    const json::json_pointer command("/commandContext/onTap/innertubeCommand/browseEndpoint/browseId");
    if (context.contains(command)) {
        playlist.author.text = avatars.at("text").at("content").get<std::string>();
        playlist.author.endpoint.browse_id = context.at(command).get<std::string>();
    } else {
        playlist.collaborators = context.at("accessibilityContext").at("label").get<std::string>();
    }
}

void from_json(const json& data, MusicPlaylist& value) {
    MusicPlaylist playlist;
    const auto& shelf = playlist_shelf(data);
    playlist.id = shelf.value("playlistId", shelf.value("targetId", std::string{}));

    auto page = playlist_tracks(shelf);
    playlist.tracks = std::move(page.tracks);
    playlist.continuation = std::move(page.continuation);

    const auto* header = playlist_header(data, playlist);
    if (header) {
        playlist.title = music_text(header->at("title"));
        playlist_author(*header, playlist);
        const auto subtitle = header->find("subtitle");
        if (subtitle != header->end() && !subtitle->at("runs").empty()) {
            const auto& year = subtitle->at("runs").back().at("text").get_ref<const std::string&>();
            if (year.size() == 4 && year.find_first_not_of("0123456789") == std::string::npos) playlist.year = year;
        }

        const json::json_pointer description("/description/musicDescriptionShelfRenderer/description");
        if (header->contains(description)) {
            playlist.description = music_text(header->at(description));
        } else if (header->contains("description")) {
            playlist.description = music_text(header->at("description"));
        }

        const auto thumbnail = header->find("thumbnail");
        if (thumbnail != header->end()) {
            const auto image = thumbnail->find("musicThumbnailRenderer");
            const auto& cover = image == thumbnail->end() ? thumbnail->at("croppedSquareThumbnailRenderer") : *image;
            playlist.thumbnails = cover.at("thumbnail").at("thumbnails").get<std::vector<MusicThumbnail>>();
        }

        const auto stats = header->find("secondSubtitle");
        if (stats != header->end() && !stats->at("runs").empty()) {
            const auto& runs = stats->at("runs");
            const bool views = runs.size() > 3;
            playlist.track_count = playlist_count(runs.at(views ? 2 : 0).at("text").get_ref<const std::string&>());
            if (views) playlist.views = runs.front().at("text").get<std::string>();
            if (runs.size() > 1) playlist.duration = runs.back().at("text").get<std::string>();
        }
    } else if (!playlist.tracks.empty()) {
        // audio playlists may omit the header, their first track still carries album metadata
        const auto& first = playlist.tracks.front();
        playlist.title = first.album.text;
        playlist.thumbnails = first.thumbnails;
        if (!first.subtitle.empty()) playlist.author = first.subtitle.front();
        if (!playlist.continuation) playlist.track_count = static_cast<std::int64_t>(playlist.tracks.size());
    }

    value = std::move(playlist);
}

void to_json(json& data, const MusicPlaylist& value) {
    data = {
        {"id", value.id},
        {"title", value.title},
        {"description", value.description},
        {"author", value.author},
        {"collaborators", value.collaborators},
        {"year", value.year},
        {"privacy", value.privacy},
        {"duration", value.duration},
        {"views", value.views},
        {"owned", value.owned},
        {"thumbnails", value.thumbnails},
        {"tracks", value.tracks}
    };
    if (value.track_count) data["track_count"] = *value.track_count;
    if (value.continuation) data["continuation"] = *value.continuation;
}
