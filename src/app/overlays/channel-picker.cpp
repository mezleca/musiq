#include "channel-picker.hpp"
#include "../widgets/button.hpp"
#include "../widgets/text.hpp"
#include "../../youtube/session.hpp"

#include <utility>

using namespace ui;

ChannelPickerOverlay::ChannelPickerOverlay(std::function<void(std::string)> select, std::function<void()> cancel)
    : ModalOverlay("channel-picker"), m_select(std::move(select)) {
    set_type_name("ChannelPickerOverlay");
    auto& panel = content();
    panel.add<MusiqText>("Choose your YouTube channel", true);
    m_status = &panel.add<MusiqText>("Select the channel to use in musiq.");
    m_channels = &panel.add<Container>("identities", StackDirection::Vertical);
    m_channels->set_size({grow(), fit()});
    m_channels->set_spacing(8.0f);
    auto& close = panel.add<MusiqButtonWidget>("Cancel");
    close.set_id("cancel");
    close.on_click(std::move(cancel));
}

void ChannelPickerOverlay::show(const std::vector<MusicIdentity>& identities, bool failed) {
    m_channels->clear();

    for (const auto& identity : identities) {
        auto label = identity.name;
        if (!identity.handle.empty()) label += "  " + identity.handle;

        auto& button = m_channels->add<MusiqButtonWidget>(std::move(label), LayoutSize{grow(), fit()});
        button.set_id(identity.data_sync_id);
        button.set_text_alignment({0.0f, 0.5f});
        button.on_click([this, id = identity.data_sync_id] { m_select(id); });
    }

    m_status->set_text(failed ? "Could not open this channel. Try again." : "Select the channel to use in musiq.");
    set_visible(true);
}
