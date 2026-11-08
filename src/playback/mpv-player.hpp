#pragma once

#include "audio-stream.hpp"
#include <mpv/client.h>
#include <memory>

struct MpvDeleter {
    void operator()(mpv_handle* handle) const;
};

struct PlayerState {
    double position = 0;
    double duration = 0;
    bool paused = true;
    bool buffering = false;
    bool loaded = false;
    bool ended = false;
    std::string error;
};

class MpvPlayer {
public:
    explicit MpvPlayer(bool silent = false);
    void load(AudioSource source);
    void load_file(const std::string& path);
    void restart();
    void stop();
    void pause(bool paused);
    void seek(double seconds);
    void set_volume(int volume);
    // drains events without waiting, ended is set only for a natural end in this update
    bool update();
    const PlayerState& state() const {
        return m_state;
    }

private:
    void command(const char** arguments);
    void load_command(const char** arguments);

    PlayerState m_state;
    std::int64_t m_entry_id = -1;
    AudioStream m_stream;
    // destroying mpv finishes its callbacks before the stream source is released
    std::unique_ptr<mpv_handle, MpvDeleter> m_handle;
};
