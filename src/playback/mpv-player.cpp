#include "mpv-player.hpp"
#include "../utils/log.hpp"

#include <algorithm>
#include <stdexcept>

static void check_mpv(int result) {
    if (result < 0) throw std::runtime_error(mpv_error_string(result));
}

void MpvDeleter::operator()(mpv_handle* handle) const {
    mpv_terminate_destroy(handle);
}

MpvPlayer::MpvPlayer(bool silent) : m_handle(mpv_create()) {
    if (!m_handle) {
        throw std::runtime_error("could not create libmpv");
    }

    for (const auto* option : {"config", "terminal", "ytdl", "video", "osc", "input-default-bindings"}) {
        check_mpv(mpv_set_option_string(m_handle.get(), option, "no"));
    }

    check_mpv(mpv_set_option_string(m_handle.get(), "idle", "yes"));

    // musiq:// is not detected as a network source, so auto would disable seekable caching
    check_mpv(mpv_set_option_string(m_handle.get(), "cache", "yes"));
    check_mpv(mpv_set_option_string(m_handle.get(), "cache-pause-initial", "no"));
    check_mpv(mpv_set_option_string(m_handle.get(), "cache-pause-wait", "0.3"));
    check_mpv(mpv_set_option_string(m_handle.get(), "demuxer-max-bytes", "8388608"));
    check_mpv(mpv_set_option_string(m_handle.get(), "demuxer-max-back-bytes", "8388608"));

    if (silent) {
        check_mpv(mpv_set_option_string(m_handle.get(), "ao", "null"));
    }

    check_mpv(mpv_initialize(m_handle.get()));
    m_stream.attach(m_handle.get());

    for (const auto* name : {"time-pos", "duration"}) {
        check_mpv(mpv_observe_property(m_handle.get(), 0, name, MPV_FORMAT_DOUBLE));
    }

    for (const auto* name : {"pause", "paused-for-cache"}) {
        check_mpv(mpv_observe_property(m_handle.get(), 0, name, MPV_FORMAT_FLAG));
    }

    LOG_INFO("[player] libmpv initialized");
}

void MpvPlayer::command(const char** arguments) {
    const int result = mpv_command_async(m_handle.get(), 0, arguments);
    if (result < 0) m_state.error = mpv_error_string(result);
}

void MpvPlayer::load(AudioSource source) {
    load_file(m_stream.open(std::move(source)));
}

void MpvPlayer::load_file(const std::string& path) {
    const char* arguments[] = {"loadfile", path.c_str(), "replace", nullptr};
    load_command(arguments);
}

void MpvPlayer::restart() {
    const char* arguments[] = {"playlist-play-index", "0", nullptr};
    load_command(arguments);
}

void MpvPlayer::load_command(const char** arguments) {
    m_state = {};
    m_state.buffering = true;
    m_entry_id = -1;
    command(arguments);
    pause(false);
}

void MpvPlayer::stop() {
    m_stream.clear();
    m_state = {};
    m_entry_id = -1;

    const char* arguments[] = {"stop", nullptr};
    command(arguments);
}

void MpvPlayer::pause(bool paused) {
    const char* arguments[] = {"set", "pause", paused ? "yes" : "no", nullptr};
    command(arguments);
    m_state.paused = paused;
}

void MpvPlayer::seek(double seconds) {
    // duration arrives in a separate property event, do not clamp against an unknown length
    const auto limit = m_state.duration > 0.0 ? m_state.duration : seconds;
    const auto value = std::to_string(std::clamp(seconds, 0.0, limit));
    const char* arguments[] = {"seek", value.c_str(), "absolute+exact", nullptr};
    command(arguments);
}

void MpvPlayer::set_volume(int volume) {
    const auto value = std::to_string(std::clamp(volume, 0, 100));
    const char* arguments[] = {"set", "volume", value.c_str(), nullptr};
    command(arguments);
}

bool MpvPlayer::update() {
    bool changed = false;
    m_state.ended = false;

    for (auto* event = mpv_wait_event(m_handle.get(), 0); event->event_id != MPV_EVENT_NONE;
         event = mpv_wait_event(m_handle.get(), 0)) {
        changed = true;
        switch (event->event_id) {
            case MPV_EVENT_START_FILE:
                m_entry_id = static_cast<mpv_event_start_file*>(event->data)->playlist_entry_id;
                break;
            case MPV_EVENT_FILE_LOADED:
                m_state.loaded = true;
                m_state.buffering = false;
                break;
            case MPV_EVENT_PROPERTY_CHANGE: {
                const auto& property = *static_cast<mpv_event_property*>(event->data);
                if (!property.data) break;
                const std::string_view name(property.name);
                if (name == "time-pos") {
                    m_state.position = *static_cast<double*>(property.data);
                } else if (name == "duration") {
                    m_state.duration = *static_cast<double*>(property.data);
                } else if (name == "pause") {
                    m_state.paused = *static_cast<int*>(property.data) != 0;
                } else if (name == "paused-for-cache" && m_state.loaded) {
                    m_state.buffering = *static_cast<int*>(property.data) != 0;
                }
                break;
            }
            case MPV_EVENT_END_FILE: {
                const auto& end = *static_cast<mpv_event_end_file*>(event->data);
                // load_file resets m_entry_id, so an end event from the replaced entry does not match
                if (end.playlist_entry_id != m_entry_id) break;
                m_state.loaded = false;
                m_state.buffering = false;
                m_state.ended = end.reason == MPV_END_FILE_REASON_EOF;
                if (end.reason == MPV_END_FILE_REASON_ERROR) m_state.error = mpv_error_string(end.error);
                break;
            }
            case MPV_EVENT_COMMAND_REPLY:
                if (event->error < 0) m_state.error = mpv_error_string(event->error);
                break;
            default:
                break;
        }
    }
    return changed;
}
