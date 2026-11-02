#include "settings.hpp"

#include <sqlite_orm/sqlite_orm.h>
#include <stdexcept>

using namespace sqlite_orm;

MusiqSettings* MusiqSettings::s_instance = nullptr;

struct MusiqSetting {
    std::string key;
    std::string value;
};

static auto make_musiq_storage(const std::string& path) {
    return make_storage(
        path,
        make_table("settings", make_column("key", &MusiqSetting::key, primary_key()), make_column("value", &MusiqSetting::value))
    );
}

void MusiqSettings::initialize(const std::string& path) {
    static MusiqSettings settings(path);
    s_instance = &settings;
}

MusiqSettings& MusiqSettings::instance() {
    if (!s_instance) {
        throw std::logic_error("settings must be initialized before use");
    }
    return *s_instance;
}

MusiqSettings::MusiqSettings(const std::string& path) : m_path(path) {
    auto storage = make_musiq_storage(m_path);
    storage.sync_schema();

    if (!storage.get_pointer<MusiqSetting>("volume")) {
        storage.replace(MusiqSetting{"volume", "80"});
    }
}

std::string MusiqSettings::get(const std::string& key) {
    const auto setting = make_musiq_storage(m_path).get_pointer<MusiqSetting>(key);
    return setting ? setting->value : std::string{};
}

void MusiqSettings::set(const std::string& key, const std::string& value) {
    make_musiq_storage(m_path).replace(MusiqSetting{key, value});
}

void MusiqSettings::remove(const std::string& key) {
    make_musiq_storage(m_path).remove<MusiqSetting>(key);
}
