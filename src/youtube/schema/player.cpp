#include "player.hpp"

#include <stdexcept>
#include <utility>

void to_json(json& data, const MusicPlayerRequest& value) {
    data = {{"videoId", value.video_id}, {"contentCheckOk", true}, {"racyCheckOk", true}};
    if (!value.playlist_id.empty()) data["playlistId"] = value.playlist_id;
    if (value.signature_timestamp) {
        data["playbackContext"]["contentPlaybackContext"]["signatureTimestamp"] = *value.signature_timestamp;
    }
    if (!value.po_token.empty()) data["serviceIntegrityDimensions"]["poToken"] = value.po_token;
}

void from_json(const json& data, MusicAudioTrack& value) {
    value = {data.value("isAutoDubbed", false), data.value("audioIsDefault", false)};
}

void from_json(const json& data, MusicFormat& value) {
    MusicFormat format;
    data.at("itag").get_to(format.itag);
    data.at("mimeType").get_to(format.mime_type);
    format.bitrate = music_optional_number(data, "bitrate").value_or(0);
    format.url = data.value("url", std::string{});

    const auto cipher = data.find("signatureCipher");
    format.signature_cipher = cipher == data.end() ? data.value("cipher", std::string{}) : cipher->get<std::string>();

    format.content_length = music_optional_number(data, "contentLength");
    format.audio_track = music_optional<MusicAudioTrack>(data, "audioTrack");
    format.drm_families = data.value("drmFamilies", std::vector<std::string>{});
    format.drm_track_type = data.value("drmTrackType", std::string{});

    value = std::move(format);
}

void from_json(const json& data, MusicStreamingData& value) {
    value.expires_in_seconds = music_optional_number(data, "expiresInSeconds");
    value.formats = data.value("formats", std::vector<MusicFormat>{});
    value.adaptive_formats = data.value("adaptiveFormats", std::vector<MusicFormat>{});
}

void from_json(const json& data, MusicVideoDetails& value) {
    data.at("videoId").get_to(value.video_id);
    data.at("title").get_to(value.title);
    value.author = data.value("author", std::string{});
    value.channel_id = data.value("channelId", std::string{});
    value.length_seconds = music_optional_number(data, "lengthSeconds");
    value.is_live = data.value("isLiveContent", false);

    const auto thumbnail = data.find("thumbnail");
    value.thumbnails =
        thumbnail == data.end() ? std::vector<MusicThumbnail>{} : thumbnail->at("thumbnails").get<std::vector<MusicThumbnail>>();
}

void from_json(const json& data, MusicPlayerData& value) {
    const auto& playability = data.at("playabilityStatus");
    playability.at("status").get_to(value.status);
    value.reason = playability.value("reason", std::string{});
    value.streaming_data = music_optional<MusicStreamingData>(data, "streamingData");
    value.video_details = music_optional<MusicVideoDetails>(data, "videoDetails");
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
