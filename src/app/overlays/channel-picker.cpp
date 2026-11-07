#include "channel-picker.hpp"
#include "../widgets/button.hpp"
#include "../widgets/text.hpp"
#include "../../youtube/session.hpp"

#include <utility>

using namespace ui;

ChannelPickerOverlay::ChannelPickerOverlay(std::function<void(std::string)> select)
    : LayerContainer("channel-picker", LayerMode::Popup), m_select(std::move(select)) {
    set_type_name("ChannelPickerOverlay");
    set_size({px(320), fit()});
    set_spacing(8.0f);
    set_visible(false);

    add<MusiqText>("Choose your YouTube channel", true);
    m_status = &add<MusiqText>("Could not open this channel. Try again.");
    m_status->set_visible(false);
    m_channels = &add<Container>("identities", StackDirection::Vertical);
    m_channels->set_size({grow(), fit()});
    m_channels->set_spacing(8.0f);
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

    m_status->set_visible(failed);
    set_visible(true);
    open_popup();
}

void ChannelPickerOverlay::apply_theme_defaults(const Theme& theme) {
    LayerContainer::apply_theme_defaults(theme);
    configure_all_styles([&theme](Style& style) {
        style.padding({12.0f});
        style.background_color(theme.background_secondary_color);
        style.border_radius(theme.metrics.popup_rounding);
        style.border_thickness(1.0f);
        style.border_color(theme.border_color);
    });
}
