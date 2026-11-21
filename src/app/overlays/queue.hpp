#pragma once

#include "modal.hpp"
#include "../widgets/context-menu.hpp"
#include "../widgets/track-list.hpp"
#include <cstdint>

class PlaybackController;
class ThumbnailCache;
class MusiqText;
class MusiqIconButtonWidget;

class QueueOverlay final : public ModalOverlay {
public:
    QueueOverlay(PlaybackController& playback, ThumbnailCache& thumbnails, MediaMenu menu = {});
    void show();
    void refresh(bool entries_changed = false);

private:
    PlaybackController& m_playback;
    ThumbnailCache& m_thumbnails;
    MediaMenu m_menu;
    TrackList* m_tracks;
    MusiqText* m_status;
    ui::Container* m_more_row = nullptr;
    MusiqIconButtonWidget* m_more = nullptr;
};
