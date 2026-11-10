#pragma once

#include "audio-source.hpp"
#include <memory>
#include <mutex>

struct mpv_handle;
struct mpv_stream_cb_info;
struct AudioLoad;

class AudioStream {
public:
    void attach(mpv_handle* handle);
    std::string open(AudioSource source);
    void clear();
    bool failed() const;

private:
    static int open_callback(void* owner, char* uri, mpv_stream_cb_info* info);

    mutable std::mutex m_mutex;
    std::shared_ptr<AudioLoad> m_load;
    std::uint64_t m_id = 0;
};
