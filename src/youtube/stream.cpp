#include "stream.hpp"
#include "client.hpp"
#include "config.hpp"
#include "yt-dlp.hpp"
#include <chrono>
#include <mutex>

class VisitorCache {
public:
    YoutubeResult<std::string> get(const YoutubeMusicClient& client) {
        const std::lock_guard lock(m_mutex);
        if (!m_value.empty() && std::chrono::steady_clock::now() < m_expires) return m_value;

        auto visitor = client.visitor_data();
        if (!visitor) return std::unexpected(std::move(visitor.error()));

        m_value = std::move(visitor->visitor_data);
        m_expires = std::chrono::steady_clock::now() + std::chrono::minutes(30);
        return m_value;
    }

private:
    std::mutex m_mutex;
    std::string m_value;
    std::chrono::steady_clock::time_point m_expires;
};

static VisitorCache visitor_cache;

static const MusicFormat* best_audio_format(const std::vector<MusicFormat>& formats) {
    const MusicFormat* best = nullptr;
    for (const auto& format : formats) {
        const bool direct =
            format.mime_type.starts_with("audio/") && !format.url.empty() && format.content_length.value_or(0) > 0;
        const bool drm = !format.drm_families.empty() || !format.drm_track_type.empty();
        if (!direct || drm) continue;
        if (format.audio_track && (!format.audio_track->is_default || format.audio_track->is_auto_dubbed)) continue;
        if (!best || format.bitrate > best->bitrate) best = &format;
    }
    return best;
}

const MusicFormat* select_audio_format(const MusicStreamingData& data) {
    if (const auto* adaptive = best_audio_format(data.adaptive_formats)) return adaptive;
    return best_audio_format(data.formats);
}

YoutubeResult<AudioSource> resolve_audio(const YoutubeMusicSession& session, const std::string& video_id) {
    const YoutubeMusicClient client(session);
    const auto visitor = visitor_cache.get(client);

    // anonymous VISIONOS player first, yt-dlp when no playable direct format exists
    std::string reason = visitor ? "no supported direct audio stream" : visitor.error().message;
    if (visitor) {
        auto player = client.player({video_id, {}, {}, {}}, {MusicClient::VisionOs, MusicAuth::Anonymous, *visitor});
        if (player && player->status == "OK" && player->streaming_data) {
            if (const auto* format = select_audio_format(*player->streaming_data)) {
                return AudioSource{
                    format->url, std::string(music_config::client_profile(MusicClient::VisionOs).user_agent),
                    *format->content_length
                };
            }
        } else if (!player) {
            reason = player.error().message;
        } else if (!player->reason.empty()) {
            reason = player->reason;
        }
    }

    return resolve_with_ytdlp(session, video_id, reason);
}
