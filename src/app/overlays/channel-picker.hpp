#pragma once

#include "modal.hpp"
#include <functional>
#include <string>
#include <vector>

struct MusicIdentity;
class MusiqText;

class ChannelPickerOverlay final : public ModalOverlay {
public:
    ChannelPickerOverlay(std::function<void(std::string)> select, std::function<void()> cancel);
    void show(const std::vector<MusicIdentity>& identities, bool failed = false);

private:
    ui::Container* m_channels;
    MusiqText* m_status;
    std::function<void(std::string)> m_select;
};
