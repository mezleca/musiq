#pragma once

#include <string>
#include "../playback/queue.hpp"

class MusiqSettings {
public:
    static MusiqSettings& instance();

    MusiqSettings(const MusiqSettings&) = delete;
    MusiqSettings& operator=(const MusiqSettings&) = delete;
    MusiqSettings(MusiqSettings&&) = delete;
    MusiqSettings& operator=(MusiqSettings&&) = delete;

    int volume() const;
    void set_volume(int volume);
    LoopMode repeat_mode() const;
    void set_repeat_mode(LoopMode mode);
    float interface_scale() const;
    void set_interface_scale(float scale);
    std::string main_font() const;
    std::string fallback_font() const;
    void set_fonts(const std::string& main, const std::string& fallback);

private:
    MusiqSettings();
};
