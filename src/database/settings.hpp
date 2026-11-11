#pragma once

#include <string>

class MusiqSettings {
public:
    static MusiqSettings& instance();

    MusiqSettings(const MusiqSettings&) = delete;
    MusiqSettings& operator=(const MusiqSettings&) = delete;
    MusiqSettings(MusiqSettings&&) = delete;
    MusiqSettings& operator=(MusiqSettings&&) = delete;

    int volume() const;
    void set_volume(int volume);
    std::string main_font() const;
    std::string fallback_font() const;
    void set_fonts(const std::string& main, const std::string& fallback);

private:
    MusiqSettings();
};
