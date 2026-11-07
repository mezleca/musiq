#pragma once

#include "schema/player.hpp"
#include "error.hpp"
#include "../playback/audio-source.hpp"

class YoutubeMusicSession;

const MusicFormat* select_audio_format(const MusicStreamingData& data);
YoutubeResult<AudioSource> resolve_audio(const YoutubeMusicSession& session, const std::string& video_id);
