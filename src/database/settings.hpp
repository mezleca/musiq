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

    int volume() const;
    void set_volume(int volume);
    std::string token() const;
    void set_token(const std::string& token);

private:
    explicit MusiqSettings(const std::string& path);

    static MusiqSettings* s_instance;
    std::string m_path;
};
