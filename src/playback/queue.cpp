#include "queue.hpp"

#include <algorithm>
#include <utility>

void PlaybackQueue::replace(std::vector<MusicItem> tracks, std::size_t start, std::string source) {
    m_source = std::move(source);
    m_entries.clear();
    m_entries.reserve(tracks.size());
    for (auto& track : tracks) {
        m_entries.push_back({m_next_id++, std::move(track)});
    }
    m_position = m_entries.empty() ? 0 : std::min(start, m_entries.size() - 1);
    ++m_revision;
}

void PlaybackQueue::append(MusicItem track, bool preserve_source) {
    if (!preserve_source) m_source.clear();
    m_entries.push_back({m_next_id++, std::move(track)});
    ++m_revision;
}

void PlaybackQueue::play_next(MusicItem track) {
    m_source.clear();
    const auto position = m_entries.empty() ? 0 : m_position + 1;
    m_entries.insert(m_entries.begin() + static_cast<std::ptrdiff_t>(position), {m_next_id++, std::move(track)});
    ++m_revision;
}

const QueueEntry* PlaybackQueue::current() const {
    return m_entries.empty() ? nullptr : &m_entries[m_position];
}

std::size_t PlaybackQueue::remaining() const {
    return m_entries.empty() ? 0 : m_entries.size() - m_position - 1;
}

void PlaybackQueue::select(std::uint64_t id) {
    const auto entry = std::ranges::find(m_entries, id, &QueueEntry::id);
    if (entry == m_entries.end()) return;

    const auto position = static_cast<std::size_t>(entry - m_entries.begin());
    if (position == m_position) return;

    m_position = position;
    ++m_revision;
}

bool PlaybackQueue::remove(std::uint64_t id) {
    const auto entry = std::ranges::find(m_entries, id, &QueueEntry::id);
    if (entry == m_entries.end()) return false;

    const auto position = static_cast<std::size_t>(entry - m_entries.begin());
    m_source.clear();
    m_entries.erase(entry);
    // removing a row before the cursor shifts it down, removing the last row moves the cursor back
    if (position < m_position) --m_position;
    if (m_entries.empty()) {
        m_position = 0;
    } else if (m_position == m_entries.size()) {
        --m_position;
    }
    ++m_revision;
    return true;
}

bool PlaybackQueue::move(std::uint64_t id, std::size_t position) {
    const auto entry = std::ranges::find(m_entries, id, &QueueEntry::id);
    if (entry == m_entries.end()) return false;
    position = std::min(position, m_entries.size() - 1);
    const auto from = static_cast<std::size_t>(entry - m_entries.begin());
    if (from == position) return false;

    m_source.clear();
    const auto selected = current()->id;
    auto begin = m_entries.begin();
    // the rotate shifts the selected row, reselect it by id to keep the current track playing
    if (from < position) {
        std::rotate(begin + from, begin + from + 1, begin + position + 1);
    } else {
        std::rotate(begin + position, begin + from, begin + from + 1);
    }
    ++m_revision;
    select(selected);
    return true;
}

bool PlaybackQueue::advance() {
    if (remaining() == 0) return false;
    ++m_position;
    ++m_revision;
    return true;
}

void PlaybackQueue::previous() {
    if (m_position == 0) return;
    --m_position;
    ++m_revision;
}
