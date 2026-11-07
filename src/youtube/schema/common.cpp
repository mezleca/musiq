#include "common.hpp"

#include <charconv>
#include <stdexcept>
#include <utility>

std::string music_text(const json& data) {
    if (data.empty()) return {};
    const auto simple = data.find("simpleText");
    if (simple != data.end()) return simple->get<std::string>();
    std::string text;
    for (const auto& run : data.at("runs")) {
        text += run.at("text").get<std::string>();
    }
    return text;
}

std::vector<MusicTextRun> music_text_runs(const json& data) {
    const auto runs = data.find("runs");
    if (runs != data.end()) return runs->get<std::vector<MusicTextRun>>();
    auto text = music_text(data);
    if (text.empty()) return {};
    return {{std::move(text), {}}};
}

std::int64_t music_number(const json& data) {
    if (data.is_number_integer()) return data.get<std::int64_t>();
    const auto& text = data.get_ref<const std::string&>();
    std::int64_t number;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (error != std::errc{} || end != text.data() + text.size()) throw std::invalid_argument("invalid numeric field");
    return number;
}

std::optional<std::int64_t> music_optional_number(const json& data, std::string_view key) {
    const auto field = data.find(key);
    if (field == data.end() || field->is_null()) return std::nullopt;
    return music_number(*field);
}

void from_json(const json& data, MusicThumbnail& value) {
    value = {data.at("url").get<std::string>(), data.value("width", 0), data.value("height", 0)};
}

void to_json(json& data, const MusicThumbnail& value) {
    data = {{"url", value.url}, {"width", value.width}, {"height", value.height}};
}

void from_json(const json& data, MusicEndpoint& value) {
    MusicEndpoint endpoint;
    const auto watch = data.find("watchEndpoint");
    if (watch != data.end()) {
        endpoint.video_id = watch->value("videoId", std::string{});
        endpoint.playlist_id = watch->value("playlistId", std::string{});
        endpoint.params = watch->value("params", std::string{});
    }
    const auto browse = data.find("browseEndpoint");
    if (browse != data.end()) {
        endpoint.browse_id = browse->at("browseId").get<std::string>();
        endpoint.params = browse->value("params", std::string{});
        const json::json_pointer page_type("/browseEndpointContextSupportedConfigs/browseEndpointContextMusicConfig/pageType");
        if (browse->contains(page_type)) endpoint.page_type = browse->at(page_type).get<std::string>();
    }
    value = std::move(endpoint);
}

void to_json(json& data, const MusicEndpoint& value) {
    data = {
        {"video_id", value.video_id},
        {"playlist_id", value.playlist_id},
        {"browse_id", value.browse_id},
        {"params", value.params},
        {"page_type", value.page_type}
    };
}

void from_json(const json& data, MusicTextRun& value) {
    value.text = data.at("text").get<std::string>();
    value.endpoint = music_optional<MusicEndpoint>(data, "navigationEndpoint").value_or(MusicEndpoint{});
}

void to_json(json& data, const MusicTextRun& value) {
    data = {{"text", value.text}, {"endpoint", value.endpoint}};
}

void from_json(const json& data, MusicItem& value) {
    MusicItem item;
    item.available = data.value("musicItemRendererDisplayPolicy", std::string{}) != "MUSIC_ITEM_RENDERER_DISPLAY_POLICY_GREY_OUT";
    const auto columns = data.find("flexColumns");

    if (columns != data.end()) {
        const auto& first = columns->at(0).at("musicResponsiveListItemFlexColumnRenderer").at("text");
        item.title = music_text(first);
        const auto title_runs = first.find("runs");
        if (title_runs != first.end() && !title_runs->empty()) {
            item.endpoint = music_optional<MusicEndpoint>(title_runs->front(), "navigationEndpoint").value_or(MusicEndpoint{});
        }
        if (columns->size() > 1) {
            const auto& text = columns->at(1).at("musicResponsiveListItemFlexColumnRenderer").at("text");
            item.subtitle = music_text_runs(text);
        }

        const auto fixed = data.find("fixedColumns");
        if (fixed != data.end() && !fixed->empty()) {
            item.duration = music_text(fixed->at(0).at("musicResponsiveListItemFixedColumnRenderer").at("text"));
        }

        for (std::size_t index = 2; index < columns->size(); ++index) {
            const auto& text = columns->at(index).at("musicResponsiveListItemFlexColumnRenderer").at("text");
            const auto runs = text.find("runs");
            if (runs == text.end() || runs->empty()) continue;

            auto run = runs->front().get<MusicTextRun>();
            if (is_album(run.endpoint)) {
                item.album = std::move(run);
            } else if (
                item.duration.empty() && run.text.contains(':') && run.text.find_first_not_of("0123456789:") == std::string::npos
            ) {
                item.duration = std::move(run.text);
            }
        }
    } else {
        // two-row cards for albums and playlists use title, subtitle and thumbnailRenderer
        item.title = music_text(data.at("title"));
        const auto subtitle = data.find("subtitle");
        if (subtitle != data.end()) {
            item.subtitle = music_text_runs(*subtitle);
        }
    }

    auto navigation = data.find("navigationEndpoint");
    if (navigation == data.end()) navigation = data.find("onTap");
    if (navigation != data.end()) item.endpoint = navigation->get<MusicEndpoint>();

    const auto playlist_item = data.find("playlistItemData");
    if (playlist_item != data.end()) {
        item.endpoint.video_id = playlist_item->value("videoId", item.endpoint.video_id);
        item.playlist_set_video_id = playlist_item->value("playlistSetVideoId", std::string{});
    }

    auto image = data.find("thumbnail");
    if (image == data.end()) image = data.find("thumbnailRenderer");
    if (image != data.end()) {
        item.thumbnails = image->at("musicThumbnailRenderer").at("thumbnail").at("thumbnails").get<std::vector<MusicThumbnail>>();
    }

    value = std::move(item);
}

void to_json(json& data, const MusicItem& value) {
    data = {{"title", value.title},       {"subtitle", value.subtitle},
            {"endpoint", value.endpoint}, {"thumbnails", value.thumbnails},
            {"duration", value.duration}, {"playlist_set_video_id", value.playlist_set_video_id},
            {"album", value.album},       {"available", value.available}};
}
