#include "settings.hpp"
#include "../widgets/text.hpp"
#include "../widgets/button.hpp"
#include "../../database/settings.hpp"
#include "../../utils/log.hpp"

#include <imgui-ui/surface.hpp>
#include <imgui-ui/widgets/number-input.hpp>
#include <algorithm>
#include <exception>
#include <utility>

using namespace ui;

class SettingsSectionWidget final : public Container {
public:
    explicit SettingsSectionWidget(std::string id) : Container(std::move(id), StackDirection::Vertical) {
        set_type_name("SettingsSectionWidget");
        set_size({grow(), fit()});
        set_spacing(12.0f);
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        Container::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) {
            style.padding({12.0f});
            style.background_color(theme.background_secondary_color);
            style.border_radius(6.0f);
            style.border_thickness(0.0f);
        });
    }
};

SettingsOverlay::SettingsOverlay(std::function<void()> log_out) : Container("settings", StackDirection::Vertical) {
    set_type_name("SettingsOverlay");
    set_size({grow(), grow()});
    set_spacing(12.0f);
    set_scrollable(true);
    set_visible(false);

    auto& settings = MusiqSettings::instance();

    try {
        m_volume = settings.volume();
    } catch (const std::exception& error) {
        LOG_WARN("[settings] could not read volume: {}", error.what());
    }

    add<MusiqText>("Settings", true);
    m_status = &add<MusiqText>("Changes are saved automatically.");

    auto& audio = add<SettingsSectionWidget>("audio");
    audio.add<MusiqText>("Audio", true);

    auto& volume = audio.add<NumberInputWidget<int>>(m_volume, "Volume", LabelPlacement::Above, "volume");
    volume.set_size({grow(), fit()});
    volume.set_range(0, 100);
    volume.set_format("%d%%");
    volume.set_flags(ImGuiSliderFlags_AlwaysClamp);
    volume.on_change([this] { save_volume(); });

    m_account = &add<SettingsSectionWidget>("account");
    m_account->add<MusiqText>("Account", true);

    auto& logout = m_account->add<MusiqButtonWidget>("Log out");
    logout.set_id("log-out");
    logout.on_click([this, log_out = std::move(log_out)] {
        try {
            log_out();
        } catch (const std::exception& error) {
            LOG_ERROR("[settings] could not log out: {}", error.what());
            m_status->set_text("Could not log out. Try again.");
        }
    });
    set_authenticated(false);
}

void SettingsOverlay::set_authenticated(bool authenticated) {
    m_account->set_visible(authenticated);
}

void SettingsOverlay::save_volume() {
    auto& settings = MusiqSettings::instance();
    m_volume = std::clamp(m_volume, 0, 100);

    try {
        settings.set_volume(m_volume);
        m_status->set_text("Changes are saved automatically.");
    } catch (const std::exception& error) {
        LOG_ERROR("[settings] could not save volume: {}", error.what());
        m_status->set_text("Could not save changes. Try again.");
    }
}

void SettingsOverlay::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);

    configure_all_styles([&theme](Style& style) {
        style.padding({16.0f});
        style.background_color(theme.background_color);
        style.border_radius(0.0f);
    });
}
