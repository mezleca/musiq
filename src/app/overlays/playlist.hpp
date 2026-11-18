#pragma once

#include <arbor/layout/container.hpp>
#include "../../youtube/playlist-browser.hpp"
#include "../widgets/track-list.hpp"

#include <functional>

class ThumbnailCache;
class PlaybackController;
class MediaCoverWidget;
class MusiqText;
class MusiqButtonWidget;
class MusiqIconButtonWidget;

class PlaylistOverlay final : public ui::Container {
public:
    explicit PlaylistOverlay(
        MusicItem item, ThumbnailCache& thumbnails, PlaybackController& playback,
        std::function<void(const MusicPlaylist&)> edit_playlist = {}
    );
    void load(const YoutubeMusicSession& session, bool reload = false);
    void invalidate();
    bool poll();
    void update_playback();
    const std::string& browse_id() const {
        return m_item.endpoint.browse_id;
    }

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void refresh();
    void play(std::size_t start);

    MusicItem m_item;
    ThumbnailCache& m_thumbnails;
    PlaybackController& m_playback;
    PlaylistBrowser m_browser;
    MediaCoverWidget* m_cover;
    MusiqText* m_title;
    MusiqText* m_author;
    MusiqText* m_details;
    MusiqText* m_description;
    MusiqText* m_status;
    MusiqIconButtonWidget* m_play;
    MusiqIconButtonWidget* m_edit;
    TrackList* m_tracks;
    std::function<void(const MusicPlaylist&)> m_edit_playlist;
};
