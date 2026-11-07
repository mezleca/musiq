#pragma once

#include <imgui-ui/layout/container.hpp>
#include "../../youtube/schema/playlist.hpp"
#include "../../youtube/error.hpp"
#include "../../youtube/session.hpp"
#include <future>
#include <memory>
#include <unordered_map>

class ThreadPool;
class ThumbnailCache;
class PlaybackController;
class MediaCoverWidget;
class MediaCardWidget;
class MusiqText;
class MusiqButtonWidget;
namespace ui {
    class VirtualLayout;
}

class PlaylistOverlay final : public ui::Container {
public:
    PlaylistOverlay(MusicItem item, ThumbnailCache& thumbnails, ThreadPool& workers, PlaybackController& playback);
    void load(const YoutubeMusicSession& session, bool reload = false);
    void invalidate();
    void poll();
    void update_playback();

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void accept_page(MusicPlaylist page);
    void refresh();
    void play(std::size_t start);
    void load_more();

    MusicItem m_item;
    ThumbnailCache& m_thumbnails;
    ThreadPool& m_workers;
    PlaybackController& m_playback;
    MusicPlaylist m_playlist;
    std::future<YoutubeResult<MusicPlaylist>> m_task;
    std::shared_ptr<const YoutubeMusicSession> m_session;
    MediaCoverWidget* m_cover;
    MusiqText* m_title;
    MusiqText* m_author;
    MusiqText* m_details;
    MusiqText* m_description;
    MusiqText* m_status;
    MusiqButtonWidget* m_play;
    MusiqButtonWidget* m_more;
    ui::VirtualLayout* m_tracks;
    std::unordered_map<std::size_t, MediaCardWidget*> m_rows;
    bool m_appending = false;
    bool m_loaded = false;
};
