#pragma once

#include <string>
#include <string_view>

class MusiqCache {
public:
    static MusiqCache& instance();

    MusiqCache(const MusiqCache&) = delete;
    MusiqCache& operator=(const MusiqCache&) = delete;
    MusiqCache(MusiqCache&&) = delete;
    MusiqCache& operator=(MusiqCache&&) = delete;

    std::string get(const std::string& key) const;
    void set(const std::string& key, std::string_view value);
    void erase_prefix(const std::string& prefix);

private:
    MusiqCache();
};
