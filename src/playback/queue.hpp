#pragma once

#include "../youtube/schema/common.hpp"
#include <functional>

struct QueueEntry {
    std::uint64_t id;
    MusicItem media;
};

enum class LoopMode {
    None,
    Once,
    Track,
    Playlist
};

class PlaybackQueue {
public:
    void on_change(std::function<void()> callback);
    void replace(std::vector<MusicItem> tracks, std::size_t start = 0, std::string source = {});
    void play_next(MusicItem track);
    void append(MusicItem track, bool preserve_source = false);
    bool remove(std::uint64_t id);
    void select(std::uint64_t id);
    bool advance();
    bool can_advance() const;
    void previous();
    void set_random(bool enabled);
    void set_repeat(bool enabled);
    void set_loop(LoopMode mode);
    bool take_repeat();

    bool random() const {
        return m_random;
    }

    bool repeat() const {
        return m_loop != LoopMode::None;
    }

    LoopMode loop() const {
        return m_loop;
    }

    const QueueEntry* current() const;
    std::size_t remaining() const;

    std::size_t position() const {
        return m_position;
    }

    const std::vector<QueueEntry>& entries() const {
        return m_entries;
    }

    const std::string& source() const {
        return m_source;
    }

private:
    void changed();
    std::vector<QueueEntry> m_entries;
    std::function<void()> m_on_change;

    std::string m_source;
    std::size_t m_position = 0;
    std::uint64_t m_next_id = 1;
    bool m_random = false;
    LoopMode m_loop = LoopMode::None;
};
