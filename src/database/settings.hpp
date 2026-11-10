#pragma once

class MusiqSettings {
public:
    static MusiqSettings& instance();

    MusiqSettings(const MusiqSettings&) = delete;
    MusiqSettings& operator=(const MusiqSettings&) = delete;
    MusiqSettings(MusiqSettings&&) = delete;
    MusiqSettings& operator=(MusiqSettings&&) = delete;

    int volume() const;
    void set_volume(int volume);

private:
    MusiqSettings();
};
