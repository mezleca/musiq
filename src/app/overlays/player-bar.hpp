#pragma once

#include <imgui-ui/layout/container.hpp>
#include <functional>
#include <cstdint>
#include "../widgets/button.hpp"

class MusiqText;
class MediaCoverWidget;
class ThumbnailCache;
class PlaybackController;
class MpvPlayer;
class SpinnerWidget;
class MusiqSliderWidget;

class QueueButtonWidget final : public MusiqIconButtonWidget {
public:
    QueueButtonWidget();

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
};

class PlayerBarOverlay final : public ui::Container {
public:
    PlayerBarOverlay(
        ThumbnailCache& thumbnails, MpvPlayer& player, PlaybackController& playback, std::function<void(int)> volume_changed
    );
    void refresh();
    void set_volume(int volume) {
        m_volume = volume;
    }

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    MpvPlayer& m_player;
    PlaybackController& m_playback;
    MusiqText* m_title;
    MusiqText* m_artist;
    MusiqText* m_duration;
    MusiqText* m_position;
    MusiqText* m_status;
    ui::Container* m_selection;
    MediaCoverWidget* m_cover;
    MusiqButtonWidget* m_previous;
    MusiqButtonWidget* m_next;
    MusiqIconButtonWidget* m_play;
    SpinnerWidget* m_spinner;
    MusiqSliderWidget* m_seek;
    std::uint64_t m_current_id = 0;
    double m_volume = 80;
    double m_progress = 0;
    int m_position_seconds = -1;
    int m_duration_seconds = -1;
};
