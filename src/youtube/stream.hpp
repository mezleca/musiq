#pragma once

#include "error.hpp"
#include "../playback/audio-source.hpp"

#include <chrono>
#include <mutex>
#include <string>

struct MusicFormat;
struct MusicStreamingData;
class YoutubeMusicClient;
class YoutubeMusicSession;

class YoutubeMediaStream {
public:
    static const MusicFormat* select_audio_format(const MusicStreamingData& data);
    YoutubeResult<AudioSource> resolve(const YoutubeMusicSession& session, const std::string& video_id);

private:
    YoutubeResult<std::string> visitor_data(const YoutubeMusicClient& client);

    std::mutex m_mutex;
    std::string m_visitor_data;
    std::chrono::steady_clock::time_point m_visitor_expires;
};
