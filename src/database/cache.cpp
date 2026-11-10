#include "cache.hpp"
#include "storage.hpp"
#include "../utils/system.hpp"

#include <vector>

using namespace sqlite_orm;

struct CacheEntry {
    std::string key;
    std::vector<char> value;
};

static auto make_cache_storage(const std::string& path) {
    return make_storage(
        path, make_table("cache", make_column("key", &CacheEntry::key, primary_key()), make_column("value", &CacheEntry::value))
    );
}

static std::unique_ptr<decltype(make_cache_storage(""))> cache_storage;

MusiqCache& MusiqCache::instance() {
    static MusiqCache cache;
    return cache;
}

MusiqCache::MusiqCache() {
    cache_storage = open_storage((utils::get_app_dir() / "cache.sqlite").string(), "cache", make_cache_storage);
    LOG_INFO("[cache] database ready");
}

std::string MusiqCache::get(const std::string& key) const {
    const auto entry = cache_storage->get_pointer<CacheEntry>(key);
    return entry ? std::string(entry->value.begin(), entry->value.end()) : std::string{};
}

void MusiqCache::set(const std::string& key, std::string_view value) {
    cache_storage->replace(CacheEntry{key, {value.begin(), value.end()}});
}

void MusiqCache::erase_prefix(const std::string& prefix) {
    cache_storage->remove_all<CacheEntry>(where(like(&CacheEntry::key, prefix + "%")));
}
