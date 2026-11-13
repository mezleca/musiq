#include "settings.hpp"
#include "../widgets/text.hpp"
#include "../widgets/button.hpp"
#include "../widgets/slider.hpp"
#include "../../database/settings.hpp"
#include "../../utils/log.hpp"

#include <arbor/widgets/dropdown.hpp>
#include <arbor/surface.hpp>
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
    FontRegistry& font_registry, std::function<void()> log_out, std::function<void(int)> volume_changed,
    std::function<void()> clear_cookies, std::function<void()> fonts_changed
)
    : Container("settings", StackDirection::Vertical), m_volume_changed(std::move(volume_changed)),
      m_fonts_changed(std::move(fonts_changed)) {
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

    m_status = &add<MusiqText>("Changes are saved automatically.");

    auto& appearance = add<SettingsSectionWidget>("appearance");
    appearance.add<MusiqText>("Appearance", FontWeight::Bold, 22.0f);

    m_interface_scale = settings.interface_scale();
    appearance.add<MusiqText>("Interface scale");
    auto& scale_row = appearance.add<Container>("interface-scale", StackDirection::Horizontal);
    scale_row.set_size({grow(), fit()});
    scale_row.set_spacing(12.0f);
    scale_row.set_content_alignment(Anchor::CenterLeft);
    auto& scale = scale_row.add<MusiqSliderWidget>(m_interface_scale);
    scale.set_range(0.75, 1.5);
    m_scale_text = &scale_row.add<MusiqText>(std::format("{:.2f}x", m_interface_scale));
    scale.on_change([this] {
        m_scale_text->set_text(std::format("{:.2f}x", m_interface_scale));
        m_scale_debounce.restart();
    });

    m_main_font = settings.main_font();
    m_fallback_font = settings.fallback_font();
    std::vector<DropdownOption> main_options{{.label = "System Font", .value = {}, .description = {}}};
    std::vector<DropdownOption> fallback_options{{.label = "Automatic", .value = {}, .description = {}}};
    for (const auto& family : font_registry.families()) {
        main_options.push_back({.label = family.name, .value = family.id, .description = {}});
        if (family.coverage.supports(FontScript::Han) || family.coverage.supports(FontScript::Hiragana) ||
            family.coverage.supports(FontScript::Katakana) || family.coverage.supports(FontScript::Hangul)) {
            fallback_options.push_back({.label = family.name, .value = family.id, .description = {}});
        }
    }

    auto& main_font = appearance.add<DropdownWidget>(m_main_font, std::move(main_options), "Main font");
    main_font.set_size({grow(), fit()});
    main_font.on_select([this] { save_fonts(); });
    auto& fallback_font = appearance.add<DropdownWidget>(m_fallback_font, std::move(fallback_options), "Fallback font");
    fallback_font.set_size({grow(), fit()});
    fallback_font.on_select([this] { save_fonts(); });

    auto& audio = add<SettingsSectionWidget>("audio");
    audio.add<MusiqText>("Audio", FontWeight::Bold, 22.0f);

    audio.add<MusiqText>("Volume");
    auto& row = audio.add<Container>("volume", StackDirection::Horizontal);
    row.set_size({grow(), fit()});
    row.set_spacing(12.0f);
    row.set_content_alignment(Anchor::CenterLeft);
    auto& volume = row.add<MusiqSliderWidget>(m_volume);
    m_volume_text = &row.add<MusiqText>(std::format("{}%", static_cast<int>(m_volume)));
    volume.on_change([this] { save_volume(); });

    m_account = &add<SettingsSectionWidget>("account");
    m_account->add<MusiqText>("Account", FontWeight::Bold, 22.0f);

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
    browser.add<MusiqText>("Webview", FontWeight::Bold, 22.0f);
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

void SettingsOverlay::update(float dt) {
    // finish pending changes even if settings were hidden during the debounce.
    if (m_scale_debounce.poll()) save_scale();

    Container::update(dt);
}

void SettingsOverlay::set_volume(int volume) {
    m_volume = volume;
    save_volume();
}

void SettingsOverlay::save_fonts() {
    try {
        MusiqSettings::instance().set_fonts(m_main_font, m_fallback_font);
        m_fonts_changed();
        m_status->set_text("Changes are saved automatically.");
    } catch (const std::exception& error) {
        LOG_ERROR("[settings] could not save fonts: {}", error.what());
        m_status->set_text("Could not save changes. Try again.");
    }
}

void SettingsOverlay::save_scale() {
    try {
        const auto scale = static_cast<float>(m_interface_scale);
        MusiqSettings::instance().set_interface_scale(scale);
        surface().set_scale(scale);
        m_status->set_text("Changes are saved automatically.");
    } catch (const std::exception& error) {
        LOG_ERROR("[settings] could not save interface scale: {}", error.what());
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
