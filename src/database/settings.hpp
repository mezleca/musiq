#pragma once

#include <string>

class MusiqSettings {
public:
    static void initialize(const std::string& path);
    static MusiqSettings& instance();

    MusiqSettings(const MusiqSettings&) = delete;
    MusiqSettings& operator=(const MusiqSettings&) = delete;
    MusiqSettings(MusiqSettings&&) = delete;
    MusiqSettings& operator=(MusiqSettings&&) = delete;

    std::string get(const std::string& key);
    void set(const std::string& key, const std::string& value);
    void remove(const std::string& key);

private:
    explicit MusiqSettings(const std::string& path);

    static MusiqSettings* s_instance;
    std::string m_path;
};
