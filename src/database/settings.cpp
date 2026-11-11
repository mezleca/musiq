#include "settings.hpp"
#include "storage.hpp"
#include "../utils/system.hpp"
#include "../utils/log.hpp"

#include <sqlite_orm/sqlite_orm.h>
#include <algorithm>
#include <memory>

using namespace sqlite_orm;

struct SettingsData {
    int id = 1;
    int volume = 80;
    std::string main_font;
    std::string fallback_font;
};

static auto make_musiq_storage(const std::string& path) {
    return make_storage(
        path, make_table(
                  "settings", make_column("id", &SettingsData::id, primary_key()), make_column("volume", &SettingsData::volume),
                  make_column("main_font", &SettingsData::main_font, default_value("")),
                  make_column("fallback_font", &SettingsData::fallback_font, default_value(""))
              )
    );
}

using SettingsStorage = decltype(make_musiq_storage(""));

static std::unique_ptr<SettingsStorage> settings_storage;

MusiqSettings& MusiqSettings::instance() {
    static MusiqSettings settings;
    return settings;
}

MusiqSettings::MusiqSettings() {
    settings_storage = open_storage((utils::get_app_dir() / "settings.sqlite").string(), "settings", make_musiq_storage);
    if (!settings_storage->get_pointer<SettingsData>(1)) {
        settings_storage->replace(SettingsData{});
    }

    LOG_INFO("[settings] database ready");
}

int MusiqSettings::volume() const {
    return settings_storage->get<SettingsData>(1).volume;
}

void MusiqSettings::set_volume(int volume) {
    settings_storage->update_all(set(c(&SettingsData::volume) = std::clamp(volume, 0, 100)), where(c(&SettingsData::id) == 1));
}

std::string MusiqSettings::main_font() const {
    return settings_storage->get<SettingsData>(1).main_font;
}

std::string MusiqSettings::fallback_font() const {
    return settings_storage->get<SettingsData>(1).fallback_font;
}

void MusiqSettings::set_fonts(const std::string& main, const std::string& fallback) {
    settings_storage->update_all(
        set(c(&SettingsData::main_font) = main, c(&SettingsData::fallback_font) = fallback), where(c(&SettingsData::id) == 1)
    );
}
