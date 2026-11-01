#pragma once

#include <string>

class MusiqSettings {
public:
    explicit MusiqSettings(const std::string& path);

    std::string get(const std::string& key);
    void set(const std::string& key, const std::string& value);
    void remove(const std::string& key);

private:
    std::string m_path;
};
