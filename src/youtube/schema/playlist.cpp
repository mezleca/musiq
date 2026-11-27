#include "playlist.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <utility>

std::string playlist_browse_id(std::string_view playlist_id) {
    return playlist_id.starts_with("VL") ? std::string(playlist_id) : "VL" + std::string(playlist_id);
}

std::string_view playlist_write_id(std::string_view playlist_id) {
    return playlist_id.starts_with("VL") ? playlist_id.substr(2) : playlist_id;
}

bool is_liked_music_playlist(std::string_view playlist_id) {
    return playlist_write_id(playlist_id) == "LM";
}

void to_json(json& data, const MusicPlaylistRequest& value) {
    data = {{"browseId", playlist_browse_id(value.playlist_id)}};
}

std::string_view playlist_privacy_value(MusicPlaylistPrivacy privacy) {
    switch (privacy) {
        case MusicPlaylistPrivacy::Private:
            return "PRIVATE";
        case MusicPlaylistPrivacy::Unlisted:
            return "UNLISTED";
        case MusicPlaylistPrivacy::Public:
            return "PUBLIC";
    }

    return "PRIVATE";
}

std::optional<MusicPlaylistPrivacy> playlist_privacy_from(std::string_view value) {
    if (value == "PRIVATE") return MusicPlaylistPrivacy::Private;
    if (value == "UNLISTED") return MusicPlaylistPrivacy::Unlisted;
    if (value == "PUBLIC") return MusicPlaylistPrivacy::Public;
    return std::nullopt;
}

void to_json(json& data, const MusicPlaylistCreateRequest& value) {
    data = {{"title", value.title}, {"privacyStatus", std::string(playlist_privacy_value(value.privacy))}};
    if (!value.description.empty()) data["description"] = value.description;
    if (!value.video_ids.empty()) data["videoIds"] = value.video_ids;
}

void to_json(json& data, const MusicPlaylistAddRequest& value) {
    data = {{"playlistId", playlist_write_id(value.playlist_id)}, {"actions", json::array()}};
    for (const auto& video_id : value.video_ids) {
        json action{{"action", "ACTION_ADD_VIDEO"}, {"addedVideoId", video_id}};
        // skip the server's duplicate rejection when duplicates were explicitly allowed
        if (value.allow_duplicates) action["dedupeOption"] = "DEDUPE_OPTION_SKIP";
        data["actions"].push_back(std::move(action));
    }
}

void to_json(json& data, const MusicPlaylistRemoveRequest& value) {
    data = {{"playlistId", playlist_write_id(value.playlist_id)}, {"actions", json::array()}};
    for (const auto& entry : value.entries) {
        data["actions"].push_back(
            {{"action", "ACTION_REMOVE_VIDEO"}, {"setVideoId", entry.set_video_id}, {"removedVideoId", entry.video_id}}
        );
    }
}

void to_json(json& data, const MusicPlaylistUpdateRequest& value) {
    data = {{"playlistId", playlist_write_id(value.playlist_id)}, {"actions", json::array()}};
    if (value.title) {
        data["actions"].push_back({{"action", "ACTION_SET_PLAYLIST_NAME"}, {"playlistName", *value.title}});
    }
    if (value.description) {
        data["actions"].push_back({{"action", "ACTION_SET_PLAYLIST_DESCRIPTION"}, {"playlistDescription", *value.description}});
    }
    if (value.privacy) {
        data["actions"].push_back(
            {{"action", "ACTION_SET_PLAYLIST_PRIVACY"}, {"playlistPrivacy", std::string(playlist_privacy_value(*value.privacy))}}
        );
    }
    if (value.thumbnail_blob_id || value.remove_thumbnail) {
        const json image_key{{"type", "PLAYLIST_IMAGE_TYPE_CUSTOM_THUMBNAIL"}, {"name", "studio_square_thumbnail"}};
        if (value.remove_thumbnail) {
            data["actions"].push_back({{"action", "ACTION_REMOVE_CUSTOM_THUMBNAIL"}, {"deletedCustomThumbnail", image_key}});
            return;
        }

        data["actions"].push_back(
            {{"action", "ACTION_SET_CUSTOM_THUMBNAIL"},
             {"playlistId", playlist_write_id(value.playlist_id)},
             {"addedCustomThumbnail", {{"imageKey", image_key}, {"playlistScottyEncryptedBlobId", *value.thumbnail_blob_id}}}}
        );
    }
}

void from_json(const json& data, MusicPlaylistCreated& value) {
    value.id = data.value("playlistId", std::string{});
    value.status = data.value("status", std::string{});
}

void from_json(const json& data, MusicPlaylistEditResult& value) {
    value.status = data.value("status", std::string{});

    const auto results = data.find("playlistEditResults");
    if (results == data.end() || !results->is_array()) return;

    for (const auto& entry : *results) {
        const auto added = entry.find("playlistEditVideoAddedResultData");
        if (added == entry.end()) continue;
        value.added.push_back({added->value("videoId", std::string{}), added->value("setVideoId", std::string{})});
    }
}

static const json* find_playlist_shelf(const json& data) {
    const json::json_pointer continuation("/continuationContents/musicPlaylistShelfContinuation");
    if (data.contains(continuation)) return &data.at(continuation);

    static const auto paths = {
        json::json_pointer("/contents/twoColumnBrowseResultsRenderer/secondaryContents/sectionListRenderer/contents"),
        json::json_pointer("/contents/singleColumnBrowseResultsRenderer/tabs/0/tabRenderer/content/sectionListRenderer/contents")
    };
    for (const auto& path : paths) {
        if (!data.contains(path)) continue;
        for (const auto& section : data.at(path)) {
            const auto shelf = section.find("musicPlaylistShelfRenderer");
            if (shelf != section.end()) return &*shelf;
        }
    }

    return nullptr;
}

static const json& playlist_shelf(const json& data) {
    const auto* shelf = find_playlist_shelf(data);
    if (!shelf) throw std::invalid_argument("missing playlist tracks");
    return *shelf;
}

static MusicPlaylistPage playlist_rows(const json& rows) {
    MusicPlaylistPage page;
    for (const auto& row : rows) {
        const auto item = row.find("musicResponsiveListItemRenderer");
        if (item != row.end()) page.tracks.push_back(item->get<MusicItem>());
    }

    if (!rows.empty()) page.continuation = music_continuation_item(rows.back());
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

void from_json(const json& data, MusicPlaylistRecommendations& value) {
    const auto& sections = data.at("continuationContents").at("sectionListContinuation").at("contents");
    MusicPlaylistRecommendations recommendations;
    for (const auto& section : sections) {
        const auto shelf = section.find("musicShelfRenderer");
        if (shelf == section.end()) continue;

        recommendations.tracks = playlist_tracks(*shelf).tracks;
        // suggestion rows can carry placeholder occurrence ids, but are not playlist entries
        for (auto& track : recommendations.tracks)
            track.playlist_set_video_id.clear();
        break;
    }
    value = std::move(recommendations);
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
            const auto privacy =
                editable->at("editHeader").at("musicPlaylistEditHeaderRenderer").at("privacy").get<std::string>();
            playlist.privacy = playlist_privacy_from(privacy).value_or(MusicPlaylistPrivacy::Public);
        }

        for (const auto key : {"musicResponsiveHeaderRenderer", "musicDetailHeaderRenderer"}) {
            const auto renderer = header.find(key);
            if (renderer != header.end()) return &*renderer;
        }
    }

    const json::json_pointer editable_header(
        "/contents/twoColumnBrowseResultsRenderer/tabs/0/tabRenderer/content/sectionListRenderer/contents/0/"
        "musicEditablePlaylistDetailHeaderRenderer"
    );
    if (data.contains(editable_header)) {
        const auto& editable = data.at(editable_header);
        playlist.owned = true;
        const json::json_pointer privacy_path("/editHeader/musicPlaylistEditHeaderRenderer/privacy");
        if (editable.contains(privacy_path)) {
            playlist.privacy =
                playlist_privacy_from(editable.at(privacy_path).get<std::string>()).value_or(MusicPlaylistPrivacy::Public);
        }
        for (const auto key : {"musicResponsiveHeaderRenderer", "musicDetailHeaderRenderer"}) {
            const auto renderer = editable.at("header").find(key);
            if (renderer != editable.at("header").end()) return &*renderer;
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

static void playlist_author(const json& header, const std::vector<MusicTextRun>& subtitle, MusicPlaylist& playlist) {
    const auto strapline = header.find("straplineTextOne");
    if (strapline != header.end()) {
        auto runs = music_text_runs(*strapline);
        if (!runs.empty()) {
            playlist.author = std::move(runs.front());
            return;
        }
    }

    for (const auto& run : subtitle) {
        if (run.endpoint.browse_id.starts_with("UC")) {
            playlist.author = run;
            return;
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
    const json::json_pointer sections("/contents/twoColumnBrowseResultsRenderer/secondaryContents/sectionListRenderer");
    if (data.contains(sections)) {
        playlist.recommendations_continuation = music_continuation(data.at(sections));
    }

    const auto* shelf = find_playlist_shelf(data);
    const auto* header = playlist_header(data, playlist);
    if (!shelf && !header) throw std::invalid_argument("missing playlist tracks and header");
    if (shelf) playlist.id = shelf->value("playlistId", shelf->value("targetId", std::string{}));

    if (shelf) {
        auto page = playlist_tracks(*shelf);
        playlist.tracks = std::move(page.tracks);
        playlist.continuation = std::move(page.continuation);
    }

    if (header) {
        playlist.title = music_text(header->at("title"));
        const auto subtitle = header->find("subtitle");
        const auto subtitle_runs = subtitle == header->end() ? std::vector<MusicTextRun>{} : music_text_runs(*subtitle);
        playlist_author(*header, subtitle_runs, playlist);
        if (!subtitle_runs.empty()) {
            const auto& year = subtitle_runs.back().text;
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
        const auto runs = stats == header->end() ? std::vector<MusicTextRun>{} : music_text_runs(*stats);
        if (!runs.empty()) {
            // view counts prepend a count and separator before the song count and duration
            const bool views = runs.size() > 3;
            playlist.track_count = playlist_count(runs[views ? 2 : 0].text);
            if (views) playlist.views = runs.front().text;
            if (runs.size() > 1) playlist.duration = runs.back().text;
        }
    } else if (!playlist.tracks.empty()) {
        // audio playlists may omit the header, their first track still carries album metadata
        const auto& first = playlist.tracks.front();
        playlist.title = first.album.text;
        playlist.thumbnails = first.thumbnails;
        if (!first.subtitle.empty()) playlist.author = first.subtitle.front();
        if (!playlist.continuation) playlist.track_count = static_cast<std::int64_t>(playlist.tracks.size());
    }

    if (!playlist.thumbnails.empty() && playlist.thumbnails.front().url.contains("/pl_c/")) {
        playlist.thumbnails = {*std::ranges::max_element(playlist.thumbnails, {}, &MusicThumbnail::width)};
    }

    value = std::move(playlist);
}
