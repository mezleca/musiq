#pragma once

#include <imgui-ui/layout/container.hpp>
#include <functional>

class MusiqText;

class SettingsOverlay final : public ui::Container {
public:
    SettingsOverlay(std::function<void()> log_out, std::function<void(int)> volume_changed, std::function<void()> clear_cookies);
    void set_authenticated(bool authenticated);
    void set_volume(int volume);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void save_volume();

    MusiqText* m_status;
    MusiqText* m_volume_text;
    ui::Container* m_account;
    double m_volume = 80;
    std::function<void(int)> m_volume_changed;
};
