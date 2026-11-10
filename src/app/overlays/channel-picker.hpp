#pragma once

#include <imgui-ui/layout/layer-container.hpp>
#include <functional>
#include <string>
#include <vector>

struct MusicIdentity;
class MusiqText;

class ChannelPickerOverlay final : public ui::LayerContainer {
public:
    explicit ChannelPickerOverlay(ui::Node& anchor, std::function<void(std::string)> select);

    void show(const std::vector<MusicIdentity>& identities, bool failed = false);
    void refresh(const std::vector<MusicIdentity>& identities, bool failed = false);
    bool take_dismissed();

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;
    bool paint() override;

private:
    ui::Node& m_anchor;
    ui::Container* m_channels;
    MusiqText* m_status;
    std::function<void(std::string)> m_select;
    bool m_dismissed = false;
};
