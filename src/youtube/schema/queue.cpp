#include "queue.hpp"

#include <utility>

void to_json(json& data, const MusicNextRequest& value) {
    data = {{"isAudioOnly", true}, {"enablePersistentPlaylistPanel", true}, {"tunerSettingValue", "AUTOMIX_SETTING_NORMAL"}};
    if (!value.video_id.empty()) data["videoId"] = value.video_id;
    if (!value.playlist_id.empty()) {
        data["playlistId"] = value.playlist_id;
    } else if (!value.video_id.empty()) {
        data["playlistId"] = "RDAMVM" + value.video_id;
    }
}

static MusicItem panel_item(const json& data) {
    MusicItem item;
    item.title = music_text(data.at("title"));
    item.endpoint.video_id = data.at("videoId").get<std::string>();
    if (const auto duration = data.find("lengthText"); duration != data.end()) item.duration = music_text(*duration);
    if (const auto byline = data.find("longBylineText"); byline != data.end()) {
        item.subtitle = music_text_runs(*byline);
    }
    if (const auto thumbnail = data.find("thumbnail"); thumbnail != data.end()) {
        item.thumbnails = thumbnail->at("thumbnails").get<std::vector<MusicThumbnail>>();
    }
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

    page.continuation = music_continuation(*panel);
    for (const auto& row : panel->at("contents")) {
        const json* item = nullptr;
        const auto direct = row.find("playlistPanelVideoRenderer");
        if (direct != row.end()) {
            item = &*direct;
        } else {
            const auto wrapper = row.find("playlistPanelVideoWrapperRenderer");
            if (wrapper == row.end()) continue;
            item = &wrapper->at("primaryRenderer").at("playlistPanelVideoRenderer");
        }
        if (!item->contains("unplayableText")) page.items.push_back(panel_item(*item));
    }

    value = std::move(page);
}

void to_json(json& data, const MusicNextPage& value) {
    data = {{"items", value.items}, {"lyrics", value.lyrics}, {"related", value.related}};
    if (value.continuation) data["continuation"] = *value.continuation;
}
