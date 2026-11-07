#pragma once

#include "error.hpp"
#include "../playback/audio-source.hpp"

class YoutubeMusicSession;

YoutubeResult<AudioSource>
resolve_with_ytdlp(const YoutubeMusicSession& session, const std::string& video_id, const std::string& reason);
