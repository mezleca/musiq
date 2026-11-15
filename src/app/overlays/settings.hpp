#pragma once

#include "../../utils/debounce.hpp"
#include "../../playback/queue.hpp"

#include <arbor/layout/container.hpp>
#include <functional>
#include <string>

class MusiqText;

class SettingsOverlay final : public ui::Container {
public:
    SettingsOverlay(
        ui::FontRegistry& fonts, std::function<void()> log_out, std::function<void(int)> volume_changed,
        std::function<void()> clear_cookies, std::function<void()> fonts_changed, PlaybackQueue& queue
    );
    void set_authenticated(bool authenticated);
    void set_volume(int volume);
    void refresh_playback();
    void update(float dt) override;

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void save_volume();
    void save_fonts();
    void save_scale();
    void save_repeat_mode();
    void update_repeat_description();

    MusiqText* m_status;
    MusiqText* m_volume_text;
    MusiqText* m_scale_text;
    MusiqText* m_repeat_description;
    ui::Container* m_account;
    double m_volume = 80;
    double m_interface_scale = 1.0;
    Debounce m_scale_debounce{std::chrono::milliseconds(100)};
    std::function<void(int)> m_volume_changed;
    std::function<void()> m_fonts_changed;
    std::string m_main_font;
    std::string m_fallback_font;
    PlaybackQueue& m_queue;
    std::string m_repeat_mode;
};
