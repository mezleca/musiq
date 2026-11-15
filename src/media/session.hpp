#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#ifndef _WIN32
#include <sdbus-c++/sdbus-c++.h>
#include <map>
#endif

struct MediaMetadata {
    std::uint64_t id = 0;
    std::string title;
    std::string artist;
    std::string album;
    std::string artwork;
    double duration = 0;
};

class MediaSession {
public:
#ifdef _WIN32
    explicit MediaSession(const std::string&) {}

    void update() {}
    void set_metadata(const MediaMetadata&) {}
    void set_playing(bool) {}
    void set_position(double) {}
    void seeked(double) {}
    void set_controls(bool, bool, bool, bool) {}
    void set_shuffle(bool) {}
    void set_loop(const std::string&) {}
    void set_volume(double) {}

    void on_play(std::function<void()>) {}
    void on_pause(std::function<void()>) {}
    void on_next(std::function<void()>) {}
    void on_previous(std::function<void()>) {}
    void on_seek(std::function<void(double)>) {}
    void on_shuffle(std::function<void(bool)>) {}
    void on_loop(std::function<void(const std::string&)>) {}
    void on_volume(std::function<void(double)>) {}
#else
    explicit MediaSession(const std::string& identifier);

    void update();
    void set_metadata(const MediaMetadata& metadata);
    void set_playing(bool playing);
    void set_position(double seconds);
    void seeked(double seconds);
    void set_controls(bool playable, bool next, bool previous, bool seekable);
    void set_shuffle(bool enabled);
    void set_loop(const std::string& status);
    void set_volume(double volume);

    void on_play(std::function<void()> callback);
    void on_pause(std::function<void()> callback);
    void on_next(std::function<void()> callback);
    void on_previous(std::function<void()> callback);
    void on_seek(std::function<void(double)> callback);
    void on_shuffle(std::function<void(bool)> callback);
    void on_loop(std::function<void(const std::string&)> callback);
    void on_volume(std::function<void(double)> callback);
#endif

private:
#ifndef _WIN32
    std::unique_ptr<sdbus::IConnection> m_connection;
    std::unique_ptr<sdbus::IObject> m_object;
    std::map<std::string, sdbus::Variant> m_metadata;
    std::string m_status = "Stopped";
    std::string m_loop = "None";
    double m_volume = 1;
    bool m_shuffle = false;
    std::int64_t m_position = 0;
    double m_duration = 0;
    bool m_seekable = false;
    bool m_playable = false;
    bool m_next = false;
    bool m_previous = false;
    std::function<void()> m_play_callback;
    std::function<void()> m_pause_callback;
    std::function<void()> m_next_callback;
    std::function<void()> m_previous_callback;
    std::function<void(double)> m_seek_callback;
    std::function<void(bool)> m_shuffle_callback;
    std::function<void(const std::string&)> m_loop_callback;
    std::function<void(double)> m_volume_callback;
#endif
};
