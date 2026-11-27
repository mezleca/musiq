#include "queue.hpp"
#include "../utils/random.hpp"

#include <algorithm>
#include <utility>

void PlaybackQueue::on_change(std::function<void()> callback) {
    m_on_change = std::move(callback);
}

void PlaybackQueue::changed() {
    if (m_on_change) {
        m_on_change();
    }
}

void PlaybackQueue::replace(std::vector<MusicItem> tracks, std::size_t start, std::string source, bool from_playlist) {
    if (matches(tracks, source)) return;

    m_source = std::move(source);

    m_random_history.clear();
    m_entries.clear();
    m_entries.reserve(tracks.size());

    for (auto& track : tracks) {
        m_entries.push_back({m_next_id++, std::move(track), from_playlist ? m_source : std::string{}});
    }

    m_position = m_entries.empty() ? 0 : std::min(start, m_entries.size() - 1);
    changed();
}

void PlaybackQueue::append(MusicItem track, bool preserve_source) {
    if (!preserve_source) m_source.clear();
    m_entries.push_back({m_next_id++, std::move(track)});
    changed();
}

void PlaybackQueue::play_next(MusicItem track) {
    m_source.clear();
    const auto position = m_entries.empty() ? 0 : m_position + 1;
    m_entries.insert(m_entries.begin() + static_cast<std::ptrdiff_t>(position), {m_next_id++, std::move(track)});
    changed();
}

void PlaybackQueue::extend(std::vector<MusicItem> tracks, std::string_view playlist_source) {
    if (tracks.empty()) {
        return;
    }

    m_entries.reserve(m_entries.size() + tracks.size());
    for (auto& track : tracks) {
        m_entries.push_back({m_next_id++, std::move(track), std::string(playlist_source)});
    }

    changed();
}

const QueueEntry* PlaybackQueue::current() const {
    return m_entries.empty() ? nullptr : &m_entries[m_position];
}

bool PlaybackQueue::matches(const std::vector<MusicItem>& tracks, std::string_view source) const {
    return tracks.size() == m_entries.size() && matches_prefix(tracks, source);
}

bool PlaybackQueue::matches_prefix(const std::vector<MusicItem>& tracks, std::string_view source) const {
    return !source.empty() && m_source == source && tracks.size() <= m_entries.size() &&
           std::equal(tracks.begin(), tracks.end(), m_entries.begin(), [](const MusicItem& track, const QueueEntry& entry) {
               return track.endpoint.video_id == entry.media.endpoint.video_id &&
                      track.playlist_set_video_id == entry.media.playlist_set_video_id;
           });
}

void PlaybackQueue::select(std::uint64_t id) {
    const auto entry = std::ranges::find(m_entries, id, &QueueEntry::id);
    if (entry == m_entries.end()) return;

    const auto position = static_cast<std::size_t>(entry - m_entries.begin());
    if (position == m_position) return;

    move_to(position);
}

void PlaybackQueue::move_to(std::size_t position) {
    if (m_random && position != m_position) {
        m_random_history.push_back(m_entries[m_position].id);
    }
    m_position = position;
    changed();
}

bool PlaybackQueue::remove(std::uint64_t id) {
    const auto entry = std::ranges::find(m_entries, id, &QueueEntry::id);
    if (entry == m_entries.end()) return false;

    const auto position = static_cast<std::size_t>(entry - m_entries.begin());
    m_source.clear();
    m_entries.erase(entry);

    // removing a row before the cursor shifts it down, removing the last row moves the cursor back
    if (m_position > 0 && (position < m_position || m_position == m_entries.size())) {
        --m_position;
    }

    if (m_random) {
        std::erase(m_random_history, id);

        // removing an entry can make two visits to the same row consecutive
        const auto duplicates = std::ranges::unique(m_random_history);
        m_random_history.erase(duplicates.begin(), duplicates.end());
        if (!m_random_history.empty() && m_random_history.back() == current()->id) {
            m_random_history.pop_back();
        }
    }

    changed();
    return true;
}

bool PlaybackQueue::advance() {
    if (!can_advance()) return false;

    const auto count = m_entries.size();
    const auto offset = m_random && count > 1 ? 1 + utils::random::random_index(count - 1) : 1;
    move_to((m_position + offset) % count);
    return true;
}

bool PlaybackQueue::can_advance() const {
    if (m_loop == LoopMode::Playlist && !m_entries.empty()) return true;
    return m_random ? m_entries.size() > 1 : m_position + 1 < m_entries.size();
}

void PlaybackQueue::previous() {
    if (!can_go_previous()) return;

    if (m_random) {
        const auto entry = std::ranges::find(m_entries, m_random_history.back(), &QueueEntry::id);
        m_random_history.pop_back();
        m_position = static_cast<std::size_t>(entry - m_entries.begin());
    } else {
        --m_position;
    }
    changed();
}

bool PlaybackQueue::can_go_previous() const {
    return m_random ? !m_random_history.empty() : m_position > 0;
}

void PlaybackQueue::set_random(bool enabled) {
    if (m_random == enabled) {
        return;
    }
    m_random = enabled;
    m_random_history.clear();
    changed();
}

void PlaybackQueue::set_repeat(bool enabled) {
    set_loop(enabled && current() ? LoopMode::Once : LoopMode::None);
}

void PlaybackQueue::set_loop(LoopMode mode) {
    if (m_loop == mode) {
        return;
    }

    m_loop = mode;
    changed();
}

bool PlaybackQueue::take_repeat() {
    if (!current()) return false;
    if (m_loop == LoopMode::Once) {
        set_loop(LoopMode::None);
        return true;
    }
    return m_loop == LoopMode::Track || (m_loop == LoopMode::Playlist && m_entries.size() == 1);
}
