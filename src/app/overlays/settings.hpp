#pragma once

#include <imgui-ui/layout/container.hpp>
#include <functional>

class MusiqText;

class SettingsOverlay final : public ui::Container {
public:
    explicit SettingsOverlay(std::function<void()> log_out);
    void set_authenticated(bool authenticated);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void save_volume();

    MusiqText* m_status;
    ui::Container* m_account;
    int m_volume = 80;
};
