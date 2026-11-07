#include "settings.hpp"
#include "../utils/log.hpp"

#include <sqlite_orm/sqlite_orm.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

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

using SettingsStorage = std::remove_reference_t<decltype(make_musiq_storage(std::declval<const std::string&>()))>;

// the singleton keeps one sqlite connection for the process lifetime
static std::unique_ptr<SettingsStorage> settings_storage;

static void open_settings_storage(const std::string& path) {
    settings_storage = std::make_unique<SettingsStorage>(make_musiq_storage(path));
    auto& database = *settings_storage;

    const auto schema = database.sync_schema_simulate();
    const auto status = schema.at("settings");

    if (status != sync_schema_result::already_in_sync && status != sync_schema_result::new_table_created) {
        throw std::runtime_error("incompatible settings schema");
    }

    database.sync_schema();
    if (!database.get_pointer<SettingsData>(1)) {
        database.replace(SettingsData{});
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

MusiqSettings::MusiqSettings(const std::string& path) {
    try {
        open_settings_storage(path);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& error) {
        LOG_WARN("[settings] failed to load database: {}, recreating it", error.what());

        // close the handle before deleting so the recreation also works on windows
        settings_storage.reset();
        std::filesystem::remove(path);
        std::filesystem::remove(path + "-wal");
        std::filesystem::remove(path + "-shm");
        std::filesystem::remove(path + "-journal");
        open_settings_storage(path);
    }

    LOG_INFO("[settings] database ready");
}

int MusiqSettings::volume() const {
    return settings_storage->get<SettingsData>(1).volume;
}

void MusiqSettings::set_volume(int volume) {
    settings_storage->update_all(set(c(&SettingsData::volume) = std::clamp(volume, 0, 100)), where(c(&SettingsData::id) == 1));
}

std::string MusiqSettings::token() const {
    return settings_storage->get<SettingsData>(1).token;
}

void MusiqSettings::set_token(const std::string& token) {
    settings_storage->update_all(set(c(&SettingsData::token) = token), where(c(&SettingsData::id) == 1));
}
