#include "audio-stream.hpp"
#include "../utils/log.hpp"

#include <cpr/curlholder.h>
#include <cpr/curlmultiholder.h>
#include <mpv/stream_cb.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <format>
#include <stdexcept>
#include <utility>
#include <stop_token>

struct AudioLoad {
    explicit AudioLoad(AudioSource value) : source(std::move(value)) {}

    AudioSource source;
    std::atomic_bool failed = false;
};

class AudioReadStream {
public:
    explicit AudioReadStream(std::shared_ptr<AudioLoad> load) : m_load(std::move(load)) {
        const auto& url = m_load->source.url;
        const auto& user_agent = m_load->source.user_agent;
        curl_easy_setopt(m_http.handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(m_http.handle, CURLOPT_USERAGENT, user_agent.c_str());
        curl_easy_setopt(m_http.handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(m_http.handle, CURLOPT_MAXREDIRS, 50L);
        curl_easy_setopt(m_http.handle, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
        curl_easy_setopt(m_http.handle, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(m_http.handle, CURLOPT_ACCEPT_ENCODING, "identity");
        curl_easy_setopt(m_http.handle, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
        curl_easy_setopt(m_http.handle, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(m_http.handle, CURLOPT_LOW_SPEED_TIME, 20L);
        curl_easy_setopt(m_http.handle, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(
            m_http.handle, CURLOPT_WRITEFUNCTION, +[](char* data, std::size_t size, std::size_t count, void* owner) {
                return static_cast<AudioReadStream*>(owner)->receive({data, size * count});
            }
        );
    }

    ~AudioReadStream() {
        stop_transfer();
    }

    std::int64_t read(char* buffer, std::uint64_t count) {
        if (m_cancel.stop_requested()) return -1;
        if (m_position == m_load->source.size || count == 0) return 0;

        // retries block only the demux reader, mpv keeps playing its buffered packets
        while (m_position == m_start + static_cast<std::int64_t>(m_chunk.size())) {
            if (m_cancel.stop_requested() || m_load->failed.load()) return -1;
            try {
                if (!m_active) start_transfer();
                pump();
                if (m_position < m_start + static_cast<std::int64_t>(m_chunk.size())) break;
                if (m_active) check(curl_multi_poll(m_transfer.handle, nullptr, 0, 100, nullptr));
            } catch (const std::exception& error) {
                stop_transfer();
                if (m_cancel.stop_requested()) return -1;
                if (m_retries == 3) {
                    m_load->failed.store(true);
                    LOG_ERROR("[player] audio fetch failed after 3 retries: {}", error.what());
                } else {
                    ++m_retries;
                    LOG_WARN("[player] audio fetch retry {}/3: {}", m_retries, error.what());
                }
            }
        }

        const auto offset = static_cast<std::size_t>(m_position - m_start);
        const auto size = std::min<std::uint64_t>(count, m_chunk.size() - offset);
        std::memcpy(buffer, m_chunk.data() + offset, static_cast<std::size_t>(size));
        m_position += static_cast<std::int64_t>(size);
        return static_cast<std::int64_t>(size);
    }

    std::int64_t seek(std::int64_t offset) {
        if (offset < 0 || offset > m_load->source.size || m_cancel.stop_requested()) return MPV_ERROR_GENERIC;
        if (offset < m_start || offset > m_start + static_cast<std::int64_t>(m_chunk.size())) {
            stop_transfer();
            m_chunk.clear();
            m_start = offset;
            m_retries = 0;
        }
        m_position = offset;
        return offset;
    }

    std::int64_t size() const {
        return m_load->source.size;
    }

    void cancel() {
        m_cancel.request_stop();
    }

private:
    static void check(CURLMcode result) {
        if (result != CURLM_OK) throw std::runtime_error(curl_multi_strerror(result));
    }

    void stop_transfer() {
        if (!m_active) return;
        curl_multi_remove_handle(m_transfer.handle, m_http.handle);
        m_active = false;
    }

    void start_transfer() {
        const auto total = m_load->source.size;
        m_start = m_position;
        m_end = std::min(total, m_start + 4 * 1024 * 1024);
        m_chunk.clear();
        m_expected_range = std::format("bytes {}-{}/{}", m_start, m_end - 1, total);
        curl_easy_setopt(m_http.handle, CURLOPT_RANGE, std::format("{}-{}", m_start, m_end - 1).c_str());
        check(curl_multi_add_handle(m_transfer.handle, m_http.handle));
        m_active = true;
    }

    void pump() {
        int running = 0;
        check(curl_multi_perform(m_transfer.handle, &running));
        int remaining = 0;
        while (auto* message = curl_multi_info_read(m_transfer.handle, &remaining)) {
            if (message->msg != CURLMSG_DONE) continue;
            const auto result = message->data.result;
            stop_transfer();
            if (result != CURLE_OK) throw std::runtime_error(curl_easy_strerror(result));
            if (m_chunk.size() != static_cast<std::size_t>(m_end - m_start)) {
                throw std::runtime_error("incomplete audio range");
            }
            m_retries = 0;
        }
    }

    std::size_t receive(std::string_view bytes) noexcept {
        if (m_cancel.stop_requested()) return 0;
        try {
            long status = 0;
            curl_easy_getinfo(m_http.handle, CURLINFO_RESPONSE_CODE, &status);
            if (status >= 300 && status < 400) return bytes.size();

            // validate the final response before any bytes reach the decoder, including after redirects
            curl_header* range = nullptr;
            const bool partial = status == 206 &&
                                 curl_easy_header(m_http.handle, "content-range", 0, CURLH_HEADER, -1, &range) == CURLHE_OK &&
                                 m_expected_range == range->value;
            const bool whole = status == 200 && m_start == 0 && m_end == m_load->source.size;
            if (!partial && !whole) return 0;
            if (bytes.size() > static_cast<std::size_t>(m_end - m_start) - m_chunk.size()) return 0;
            m_chunk.append(bytes);
            return bytes.size();
        } catch (...) {
            return 0;
        }
    }

    std::shared_ptr<AudioLoad> m_load;
    cpr::CurlHolder m_http;
    cpr::CurlMultiHolder m_transfer;
    std::stop_source m_cancel;
    std::string m_chunk;
    std::string m_expected_range;
    std::int64_t m_start = 0;
    std::int64_t m_end = 0;
    std::int64_t m_position = 0;
    bool m_active = false;
    unsigned m_retries = 0;
};

void AudioStream::attach(mpv_handle* handle) {
    const auto result = mpv_stream_cb_add_ro(handle, "musiq", this, open_callback);
    if (result < 0) throw std::runtime_error(mpv_error_string(result));
}

std::string AudioStream::open(AudioSource source) {
    std::lock_guard lock(m_mutex);
    m_load = std::make_shared<AudioLoad>(std::move(source));
    return std::format("musiq://{}", ++m_id);
}

void AudioStream::clear() {
    std::lock_guard lock(m_mutex);
    m_load.reset();
}

bool AudioStream::failed() const {
    std::lock_guard lock(m_mutex);
    return m_load && m_load->failed.load();
}

int AudioStream::open_callback(void* owner, char* uri, mpv_stream_cb_info* info) {
    auto& stream = *static_cast<AudioStream*>(owner);

    try {
        std::lock_guard lock(stream.m_mutex);
        if (!stream.m_load || uri != std::format("musiq://{}", stream.m_id)) {
            return MPV_ERROR_LOADING_FAILED;
        }

        auto reader = std::make_unique<AudioReadStream>(stream.m_load);
        info->read_fn = [](void* cookie, char* data, std::uint64_t size) {
            return static_cast<AudioReadStream*>(cookie)->read(data, size);
        };
        info->seek_fn = [](void* cookie, std::int64_t offset) { return static_cast<AudioReadStream*>(cookie)->seek(offset); };
        info->size_fn = [](void* cookie) { return static_cast<AudioReadStream*>(cookie)->size(); };
        info->cancel_fn = [](void* cookie) { static_cast<AudioReadStream*>(cookie)->cancel(); };
        info->close_fn = [](void* cookie) { delete static_cast<AudioReadStream*>(cookie); };
        info->cookie = reader.release();
        return 0;
    } catch (const std::exception& error) {
        LOG_ERROR("[player] opening stream failed: {}", error.what());
        return MPV_ERROR_LOADING_FAILED;
    }
}
