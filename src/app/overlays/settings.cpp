#include "settings.hpp"
#include "../widgets/text.hpp"
#include "../widgets/button.hpp"
#include "../widgets/slider.hpp"
#include "../../database/settings.hpp"
#include "../../utils/log.hpp"

#include <exception>
#include <utility>
#include <cmath>
#include <format>

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
            style.border_radius(theme.controls.rounding);
            style.border_thickness(0.0f);
        });
    }
};

SettingsOverlay::SettingsOverlay(
    std::function<void()> log_out, std::function<void(int)> volume_changed, std::function<void()> clear_cookies
)
    : Container("settings", StackDirection::Vertical), m_volume_changed(std::move(volume_changed)) {
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

    add<MusiqText>("Settings", TextWeight::Bold);
    m_status = &add<MusiqText>("Changes are saved automatically.");

    auto& audio = add<SettingsSectionWidget>("audio");
    audio.add<MusiqText>("Audio", TextWeight::Bold);

    audio.add<MusiqText>("Volume");
    auto& row = audio.add<Container>("volume", StackDirection::Horizontal);
    row.set_size({grow(), fit()});
    row.set_spacing(12.0f);
    row.set_content_alignment(Anchor::CenterLeft);
    auto& volume = row.add<MusiqSliderWidget>(m_volume);
    m_volume_text = &row.add<MusiqText>(std::format("{}%", static_cast<int>(m_volume)));
    volume.on_change([this] { save_volume(); });

    m_account = &add<SettingsSectionWidget>("account");
    m_account->add<MusiqText>("Account", TextWeight::Bold);

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

    auto& browser = add<SettingsSectionWidget>("browser");
    browser.add<MusiqText>("Webview", TextWeight::Bold);
    browser.add<MusiqButtonWidget>("Clear cookies").on_click([this, clear_cookies = std::move(clear_cookies)] {
        try {
            clear_cookies();
            m_status->set_text("Webview cookies cleared.");
        } catch (const std::exception& error) {
            LOG_ERROR("[settings] could not clear browser cookies: {}", error.what());
            m_status->set_text("Could not clear browser cookies. Try again.");
        }
    });
}

void SettingsOverlay::set_authenticated(bool authenticated) {
    m_account->set_visible(authenticated);
}

void SettingsOverlay::save_volume() {
    auto& settings = MusiqSettings::instance();

    try {
        const auto volume = static_cast<int>(std::lround(m_volume));
        settings.set_volume(volume);
        m_volume_changed(volume);
        m_volume_text->set_text(std::format("{}%", volume));
        m_status->set_text("Changes are saved automatically.");
    } catch (const std::exception& error) {
        LOG_ERROR("[settings] could not save volume: {}", error.what());
        m_status->set_text("Could not save changes. Try again.");
    }
}

void SettingsOverlay::set_volume(int volume) {
    m_volume = volume;
    save_volume();
}

void SettingsOverlay::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);

    configure_all_styles([&theme](Style& style) {
        style.padding({16.0f});
        style.background_color(theme.background_color);
        style.border_radius(0.0f);
    });
}
