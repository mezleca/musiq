#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>

namespace ui {
    class VirtualLayout;
}

class MediaCardWidget;
class ThumbnailCache;
struct MusicItem;

class TrackList {
public:
    explicit TrackList(ui::VirtualLayout& layout);

    MediaCardWidget& add(std::uint64_t key, const MusicItem& item, ThumbnailCache& thumbnails);
    MediaCardWidget* find(std::uint64_t key) const;

    void refresh(const std::function<bool(std::uint64_t)>& keep, std::optional<std::uint64_t> current);
    void clear();

    void set_current(std::optional<std::uint64_t> key);

private:
    ui::VirtualLayout& m_layout;
    std::unordered_map<std::uint64_t, MediaCardWidget*> m_rows;
    std::optional<std::uint64_t> m_current;
};
