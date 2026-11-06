#include "player.hpp"

#include <stdexcept>
#include <utility>

static std::optional<std::int64_t> number_field(const json& data, std::string_view key) {
    const auto field = data.find(key);
    if (field == data.end() || field->is_null()) return std::nullopt;
    return music_number(*field);
}

void to_json(json& data, const MusicPlayerRequest& value) {
    data = {{"videoId", value.video_id}, {"contentCheckOk", true}, {"racyCheckOk", true}};
    if (!value.playlist_id.empty()) data["playlistId"] = value.playlist_id;
    if (value.signature_timestamp) {
        data["playbackContext"]["contentPlaybackContext"]["signatureTimestamp"] = *value.signature_timestamp;
    }
    if (!value.po_token.empty()) data["serviceIntegrityDimensions"]["poToken"] = value.po_token;
}

void from_json(const json& data, MusicAudioTrack& value) {
    value = {
        data.value("id", std::string{}), data.value("displayName", std::string{}), data.value("isAutoDubbed", false),
        data.value("audioIsDefault", false)
    };
}

void to_json(json& data, const MusicAudioTrack& value) {
    data = {{"id", value.id}, {"name", value.name}, {"is_auto_dubbed", value.is_auto_dubbed}, {"is_default", value.is_default}};
}

void from_json(const json& data, MusicFormat& value) {
    MusicFormat format;
    data.at("itag").get_to(format.itag);
    data.at("mimeType").get_to(format.mime_type);
    format.bitrate = number_field(data, "bitrate").value_or(0);
    format.url = data.value("url", std::string{});

    const auto cipher = data.find("signatureCipher");
    format.signature_cipher = cipher == data.end() ? data.value("cipher", std::string{}) : cipher->get<std::string>();

    format.content_length = number_field(data, "contentLength");
    format.duration_ms = number_field(data, "approxDurationMs");
    format.width = music_optional<int>(data, "width");
    format.height = music_optional<int>(data, "height");
    format.audio_channels = music_optional<int>(data, "audioChannels");
    format.audio_track = music_optional<MusicAudioTrack>(data, "audioTrack");
    format.drm_families = data.value("drmFamilies", std::vector<std::string>{});
    format.drm_track_type = data.value("drmTrackType", std::string{});

    value = std::move(format);
}

void to_json(json& data, const MusicFormat& value) {
    data = {
        {"itag", value.itag},
        {"mime_type", value.mime_type},
        {"bitrate", value.bitrate},
        {"url", value.url},
        {"signature_cipher", value.signature_cipher},
        {"drm_families", value.drm_families},
        {"drm_track_type", value.drm_track_type}
    };
    if (value.content_length) data["content_length"] = *value.content_length;
    if (value.duration_ms) data["duration_ms"] = *value.duration_ms;
    if (value.width) data["width"] = *value.width;
    if (value.height) data["height"] = *value.height;
    if (value.audio_channels) data["audio_channels"] = *value.audio_channels;
    if (value.audio_track) data["audio_track"] = *value.audio_track;
}

void from_json(const json& data, MusicStreamingData& value) {
    value.expires_in_seconds = number_field(data, "expiresInSeconds");
    value.formats = data.value("formats", std::vector<MusicFormat>{});
    value.adaptive_formats = data.value("adaptiveFormats", std::vector<MusicFormat>{});
    value.hls_manifest_url = data.value("hlsManifestUrl", std::string{});
    value.dash_manifest_url = data.value("dashManifestUrl", std::string{});
    value.server_abr_streaming_url = data.value("serverAbrStreamingUrl", std::string{});
}

void to_json(json& data, const MusicStreamingData& value) {
    data = {
        {"formats", value.formats},
        {"adaptive_formats", value.adaptive_formats},
        {"hls_manifest_url", value.hls_manifest_url},
        {"dash_manifest_url", value.dash_manifest_url},
        {"server_abr_streaming_url", value.server_abr_streaming_url}
    };
    if (value.expires_in_seconds) data["expires_in_seconds"] = *value.expires_in_seconds;
}

void from_json(const json& data, MusicVideoDetails& value) {
    data.at("videoId").get_to(value.video_id);
    data.at("title").get_to(value.title);
    value.author = data.value("author", std::string{});
    value.channel_id = data.value("channelId", std::string{});
    value.length_seconds = number_field(data, "lengthSeconds");
    value.is_live = data.value("isLiveContent", false);

    const auto thumbnail = data.find("thumbnail");
    value.thumbnails =
        thumbnail == data.end() ? std::vector<MusicThumbnail>{} : thumbnail->at("thumbnails").get<std::vector<MusicThumbnail>>();
}

void to_json(json& data, const MusicVideoDetails& value) {
    data = {{"video_id", value.video_id},     {"title", value.title},     {"author", value.author},
            {"channel_id", value.channel_id}, {"is_live", value.is_live}, {"thumbnails", value.thumbnails}};
    if (value.length_seconds) data["length_seconds"] = *value.length_seconds;
}

void from_json(const json& data, MusicPlayerData& value) {
    const auto& playability = data.at("playabilityStatus");
    playability.at("status").get_to(value.status);
    value.reason = playability.value("reason", std::string{});
    value.streaming_data = music_optional<MusicStreamingData>(data, "streamingData");
    value.video_details = music_optional<MusicVideoDetails>(data, "videoDetails");

    const auto context = data.find("responseContext");
    value.visitor_data = context == data.end() ? std::string{} : context->value("visitorData", std::string{});

    const json::json_pointer tracking("/playbackTracking/videostatsPlaybackUrl/baseUrl");
    value.playback_tracking_url = data.contains(tracking) ? data.at(tracking).get<std::string>() : std::string{};
}

void to_json(json& data, const MusicPlayerData& value) {
    data = {
        {"status", value.status},
        {"reason", value.reason},
        {"visitor_data", value.visitor_data},
        {"playback_tracking_url", value.playback_tracking_url}
    };
    if (value.streaming_data) data["streaming_data"] = *value.streaming_data;
    if (value.video_details) data["video_details"] = *value.video_details;
}

void from_json(const json& data, MusicVisitorData& value) {
    for (const auto& field : data.at(0).at(2)) {
        if (!field.is_string()) continue;
        const auto& text = field.get_ref<const std::string&>();
        if (text.starts_with("Cgt") || text.starts_with("Cgs")) {
            value.visitor_data = text;
            return;
        }
    }
    throw std::invalid_argument("missing visitor data");
}

void to_json(json& data, const MusicVisitorData& value) {
    data = {{"visitor_data", value.visitor_data}};
}
