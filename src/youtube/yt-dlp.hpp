#pragma once

#include "error.hpp"
#include "../playback/audio-source.hpp"

class YoutubeMusicSession;

namespace dlp {

    YoutubeResult<AudioSource>
    resolve(const YoutubeMusicSession& session, const std::string& video_id, const std::string& reason);

} // namespace dlp
