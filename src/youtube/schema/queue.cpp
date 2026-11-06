#include "queue.hpp"

#include <utility>

void to_json(json& data, const MusicNextRequest& value) {
    data = {{"isAudioOnly", true}};
    if (!value.video_id.empty()) data["videoId"] = value.video_id;
    if (!value.playlist_id.empty()) data["playlistId"] = value.playlist_id;
}

static MusicItem panel_item(const json& data) {
    MusicItem item;
    item.title = music_text(data.at("title"));
    item.endpoint.video_id = data.at("videoId").get<std::string>();
    item.duration = music_text(data.at("lengthText"));
    item.subtitle = data.at("longBylineText").at("runs").get<std::vector<MusicTextRun>>();
    item.thumbnails = data.at("thumbnail").at("thumbnails").get<std::vector<MusicThumbnail>>();
    item.playlist_set_video_id = data.value("playlistSetVideoId", std::string{});
    return item;
}

void from_json(const json& data, MusicNextPage& value) {
    MusicNextPage page;

    const json::json_pointer continuation("/continuationContents/playlistPanelContinuation");
    const json* panel;

    if (data.contains(continuation)) {
        panel = &data.at(continuation);
    } else {
        const json::json_pointer tabs_path(
            "/contents/singleColumnMusicWatchNextResultsRenderer/tabbedRenderer/watchNextTabbedResultsRenderer/tabs"
        );
        const auto& tabs = data.at(tabs_path);
        panel = &tabs.at(0).at("tabRenderer").at("content").at("musicQueueRenderer").at("content").at("playlistPanelRenderer");
        if (tabs.size() > 1) {
            page.lyrics = music_optional<MusicEndpoint>(tabs.at(1).at("tabRenderer"), "endpoint").value_or(MusicEndpoint{});
        }
        if (tabs.size() > 2) {
            page.related = music_optional<MusicEndpoint>(tabs.at(2).at("tabRenderer"), "endpoint").value_or(MusicEndpoint{});
        }
    }

    page.playlist_id = panel->value("playlistId", std::string{});
    page.continuation = music_continuation(*panel);
    for (const auto& row : panel->at("contents")) {
        const auto direct = row.find("playlistPanelVideoRenderer");
        const auto wrapper = row.find("playlistPanelVideoWrapperRenderer");
        if (direct != row.end()) {
            page.items.push_back(panel_item(*direct));
        } else if (wrapper != row.end()) {
            page.items.push_back(panel_item(wrapper->at("primaryRenderer").at("playlistPanelVideoRenderer")));
        }
    }

    value = std::move(page);
}

void to_json(json& data, const MusicNextPage& value) {
    data = {{"items", value.items}, {"playlist_id", value.playlist_id}, {"lyrics", value.lyrics}, {"related", value.related}};
    if (value.continuation) data["continuation"] = *value.continuation;
}
