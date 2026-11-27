#pragma once

#include <arbor/layout/container.hpp>
#include "../../youtube/playlist-browser.hpp"
#include "../widgets/context-menu.hpp"
#include "../widgets/track-list.hpp"

#include <functional>
#include <optional>

class ThumbnailCache;
class PlaybackController;
class MediaCoverWidget;
class MusiqText;
class MusiqButtonWidget;
class MusiqIconButtonWidget;

struct PlaylistTrackReveal {
    std::string video_id;
    std::string set_video_id;
    ui::ScrollBehavior behavior;
};

class PlaylistOverlay final : public ui::Container {
public:
    explicit PlaylistOverlay(
        MusicItem item, ThumbnailCache& thumbnails, PlaybackController& playback,
        std::function<void(const MusicPlaylist&)> edit_playlist = {}, MediaMenu menu = {}
    );
    void load(const YoutubeMusicSession& session, PlaylistLoad mode = PlaylistLoad::Normal);
    void invalidate();
    PlaylistChange poll();
    void update_playback();
    void show_action_error(std::string message);
    void reveal_track(const MusicItem& track, ui::ScrollBehavior behavior);
    const MusicPlaylist& playlist() const {
        return m_browser.playlist();
    }

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
    void draw_children() override;

private:
    void refresh();
    void play(std::size_t start);

    MusicItem m_item;
    ThumbnailCache& m_thumbnails;
    PlaybackController& m_playback;
    PlaylistBrowser m_browser;
    std::optional<PlaylistTrackReveal> m_track_reveal;
    ui::Container* m_header;
    ui::Container* m_separator;
    MediaCoverWidget* m_cover;
    MusiqText* m_title;
    MusiqText* m_author;
    MusiqText* m_details;
    MusiqText* m_description;
    MusiqText* m_status;
    MusiqIconButtonWidget* m_play;
    MusiqIconButtonWidget* m_edit;
    TrackList* m_tracks;
    ui::Container* m_recommended_section;
    MusiqText* m_recommended_status;
    TrackList* m_recommended_tracks;
    std::function<void(const MusicPlaylist&)> m_edit_playlist;
    MediaMenu m_menu;
};
