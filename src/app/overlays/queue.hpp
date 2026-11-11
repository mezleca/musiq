#pragma once

#include "modal.hpp"
#include "../widgets/track-list.hpp"
#include <cstdint>

class PlaybackController;
class ThumbnailCache;
class MusiqText;
class MusiqButtonWidget;

class QueueOverlay final : public ModalOverlay {
public:
    QueueOverlay(PlaybackController& playback, ThumbnailCache& thumbnails);
    void show();
    void refresh();

protected:
    void event(ui::UiEvent& event) override;

private:
    PlaybackController& m_playback;
    ThumbnailCache& m_thumbnails;
    TrackList* m_tracks;
    MusiqText* m_status;
    MusiqButtonWidget* m_more;
    std::uint64_t m_revision = 0;
};
