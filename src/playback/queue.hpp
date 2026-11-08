#pragma once

#include "../youtube/schema/common.hpp"

struct QueueEntry {
    std::uint64_t id;
    MusicItem media;
};

class PlaybackQueue {
public:
    void replace(std::vector<MusicItem> tracks, std::size_t start = 0, std::string source = {});
    void play_next(MusicItem track);
    void append(MusicItem track, bool preserve_source = false);
    bool remove(std::uint64_t id);
    bool move(std::uint64_t id, std::size_t position);
    void select(std::uint64_t id);
    bool advance();
    bool can_advance() const;
    void previous();
    void set_random(bool enabled);
    void set_repeat(bool enabled);
    bool take_repeat();

    bool random() const {
        return m_random;
    }

    bool repeat() const {
        return m_repeat_id != 0;
    }

    const QueueEntry* current() const;
    std::size_t remaining() const;

    std::size_t position() const {
        return m_position;
    }

    const std::vector<QueueEntry>& entries() const {
        return m_entries;
    }

    std::uint64_t revision() const {
        return m_revision;
    }

    const std::string& source() const {
        return m_source;
    }

private:
    std::vector<QueueEntry> m_entries;

    std::string m_source;
    std::size_t m_position = 0;
    std::uint64_t m_next_id = 1;
    std::uint64_t m_revision = 0;
    std::uint64_t m_repeat_id = 0;
    bool m_random = false;
};
