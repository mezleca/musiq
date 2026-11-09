#include "audio-stream.hpp"
#include "../http/http-client.hpp"
#include "../utils/log.hpp"

#include <mpv/stream_cb.h>
#include <algorithm>
#include <cstring>
#include <format>
#include <stdexcept>
#include <utility>

class AudioReadStream {
public:
    explicit AudioReadStream(std::shared_ptr<const AudioSource> source) : m_source(std::move(source)) {}

    std::int64_t read(char* buffer, std::uint64_t count) {
        if (m_failed || m_cancel.stop_requested()) return -1;
        if (m_position == m_source->size || count == 0) return 0;

        try {
            const bool cached = m_position >= m_start && m_position < m_start + static_cast<std::int64_t>(m_chunk.size());
            if (!cached && !fetch_chunk()) return -1;

            const auto offset = static_cast<std::size_t>(m_position - m_start);
            const auto size = std::min<std::uint64_t>(count, m_chunk.size() - offset);
            std::memcpy(buffer, m_chunk.data() + offset, static_cast<std::size_t>(size));
            m_position += static_cast<std::int64_t>(size);
            return static_cast<std::int64_t>(size);
        } catch (const std::exception& error) {
            m_failed = true;
            LOG_ERROR("[player] audio read failed: {}", error.what());
            return -1;
        }
    }

    std::int64_t seek(std::int64_t offset) {
        if (offset < 0 || offset > m_source->size || m_cancel.stop_requested()) return MPV_ERROR_GENERIC;
        m_position = offset;
        return offset;
    }

    std::int64_t size() const {
        return m_source->size;
    }

    void cancel() {
        m_cancel.request_stop();
    }

private:
    bool fetch_chunk() {
        // fetch one ranged block at a time, the first 64 KiB keeps mpv's probe cheap
        const std::int64_t chunk_size = m_position == 0 ? 64 * 1024 : 256 * 1024;
        m_start = m_position;
        const auto end = std::min(m_source->size, m_start + chunk_size) - 1;

        HttpRequest request;
        request.url = m_source->url;
        request.follow_redirects = true;
        request.headers = {
            {"User-Agent", m_source->user_agent},
            {"Accept-Encoding", "identity"},
            {"Range", std::format("bytes={}-{}", m_start, end)}
        };
        request.stop = m_cancel.get_token();
        request.response_limit = static_cast<std::size_t>(end - m_start + 1);

        auto response = http::request(request);

        // CDN redirects must still return exactly the requested range, or the full file for the first read
        const bool range_matches = response.status_code == 206 && response.header["content-range"] ==
                                                                      std::format("bytes {}-{}/{}", m_start, end, m_source->size);
        const bool whole_file = response.status_code == 200 && m_start == 0 && end + 1 == m_source->size;
        if (http::successful(response) && (range_matches || whole_file) && response.text.size() == request.response_limit) {
            m_chunk = std::move(response.text);
            return true;
        }

        m_failed = true;
        if (!m_cancel.stop_requested()) {
            LOG_ERROR("[player] audio read failed (status={}, bytes={})", response.status_code, response.text.size());
        }
        return false;
    }

    std::shared_ptr<const AudioSource> m_source;
    std::stop_source m_cancel;
    std::string m_chunk;
    std::int64_t m_start = 0;
    std::int64_t m_position = 0;
    bool m_failed = false;
};

void AudioStream::attach(mpv_handle* handle) {
    const auto result = mpv_stream_cb_add_ro(handle, "musiq", this, open_callback);
    if (result < 0) throw std::runtime_error(mpv_error_string(result));
}

std::string AudioStream::open(AudioSource source) {
    std::lock_guard lock(m_mutex);
    m_source = std::make_shared<AudioSource>(std::move(source));
    return std::format("musiq://{}", ++m_id);
}

void AudioStream::clear() {
    std::lock_guard lock(m_mutex);
    m_source.reset();
}

int AudioStream::open_callback(void* owner, char* uri, mpv_stream_cb_info* info) {
    auto& source = *static_cast<AudioStream*>(owner);

    try {
        std::lock_guard lock(source.m_mutex);

        // open() bumps m_id and replaces m_source, so a stale loadfile uri cannot open the new source
        if (!source.m_source || uri != std::format("musiq://{}", source.m_id)) return MPV_ERROR_LOADING_FAILED;

        auto stream = std::make_unique<AudioReadStream>(source.m_source);
        info->read_fn = [](void* cookie, char* data, std::uint64_t size) {
            return static_cast<AudioReadStream*>(cookie)->read(data, size);
        };
        info->seek_fn = [](void* cookie, std::int64_t offset) { return static_cast<AudioReadStream*>(cookie)->seek(offset); };
        info->size_fn = [](void* cookie) { return static_cast<AudioReadStream*>(cookie)->size(); };
        info->cancel_fn = [](void* cookie) { static_cast<AudioReadStream*>(cookie)->cancel(); };
        info->close_fn = [](void* cookie) { delete static_cast<AudioReadStream*>(cookie); };
        info->cookie = stream.release();
        return 0;
    } catch (const std::exception& error) {
        LOG_ERROR("[player] opening stream failed: {}", error.what());
        return MPV_ERROR_LOADING_FAILED;
    }
}
