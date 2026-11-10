#pragma once

#include "../utils/log.hpp"

#include <sqlite_orm/sqlite_orm.h>
#include <filesystem>
#include <memory>
#include <new>
#include <stdexcept>

template <typename Factory>
auto open_storage(const std::string& path, const std::string& table, Factory factory) {
    const auto open = [&] {
        auto storage = std::make_unique<decltype(factory(path))>(factory(path));
        const auto status = storage->sync_schema_simulate().at(table);
        if (status != sqlite_orm::sync_schema_result::already_in_sync &&
            status != sqlite_orm::sync_schema_result::new_table_created) {
            throw std::runtime_error("incompatible database schema");
        }
        storage->sync_schema();
        return storage;
    };

    try {
        return open();
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& error) {
        LOG_WARN("[database] failed to load {}: {}, recreating it", path, error.what());

        // the failed storage is already closed before removing sqlite's files on windows
        for (const auto* suffix : {"", "-wal", "-shm", "-journal"}) {
            std::filesystem::remove(path + suffix);
        }
        return open();
    }
}
