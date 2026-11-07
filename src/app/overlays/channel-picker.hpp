#pragma once

#include <imgui-ui/layout/layer-container.hpp>
#include <functional>
#include <string>
#include <vector>

struct MusicIdentity;
class MusiqText;

class ChannelPickerOverlay final : public ui::LayerContainer {
public:
    explicit ChannelPickerOverlay(std::function<void(std::string)> select);
    void show(const std::vector<MusicIdentity>& identities, bool failed = false);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    ui::Container* m_channels;
    MusiqText* m_status;
    std::function<void(std::string)> m_select;
};
