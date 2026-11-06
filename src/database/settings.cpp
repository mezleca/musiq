#include "settings.hpp"
#include "../utils/log.hpp"

#include <sqlite_orm/sqlite_orm.h>
#include <algorithm>
#include <filesystem>
#include <stdexcept>

using namespace sqlite_orm;

MusiqSettings* MusiqSettings::s_instance = nullptr;

struct SettingsData {
    int id = 1;
    int volume = 80;
    std::string token;
};

static auto make_musiq_storage(const std::string& path) {
    return make_storage(
        path, make_table(
                  "settings", make_column("id", &SettingsData::id, primary_key()), make_column("volume", &SettingsData::volume),
                  make_column("token", &SettingsData::token)
              )
    );
}

static void load_database(const std::string& path) {
    auto storage = make_musiq_storage(path);

    const auto schema = storage.sync_schema_simulate();
    const auto status = schema.at("settings");

    if (status != sync_schema_result::already_in_sync && status != sync_schema_result::new_table_created) {
        throw std::runtime_error("incompatible settings schema");
    }

    storage.sync_schema();
    if (!storage.get_pointer<SettingsData>(1)) {
        storage.replace(SettingsData{});
    }
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
    try {
        load_database(m_path);
    } catch (const std::exception& error) {
        LOG_WARN("[settings] failed to load database: {}, recreating it", error.what());
        std::filesystem::remove(m_path);
        std::filesystem::remove(m_path + "-wal");
        std::filesystem::remove(m_path + "-shm");
        std::filesystem::remove(m_path + "-journal");
        load_database(m_path);
    }

    LOG_INFO("[settings] database ready");
}

int MusiqSettings::volume() const {
    return make_musiq_storage(m_path).get<SettingsData>(1).volume;
}

void MusiqSettings::set_volume(int volume) {
    make_musiq_storage(m_path).update_all(
        set(c(&SettingsData::volume) = std::clamp(volume, 0, 100)), where(c(&SettingsData::id) == 1)
    );
}

std::string MusiqSettings::token() const {
    return make_musiq_storage(m_path).get<SettingsData>(1).token;
}

void MusiqSettings::set_token(const std::string& token) {
    make_musiq_storage(m_path).update_all(set(c(&SettingsData::token) = token), where(c(&SettingsData::id) == 1));
}
