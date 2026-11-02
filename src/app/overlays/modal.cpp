#include "modal.hpp"

#include <utility>

using namespace ui;

ModalOverlay::ModalOverlay(std::string id) : LayerContainer(std::move(id)) {
    set_type_name("ModalOverlay");
    set_visible(false);
    set_input_mode(InputMode::Blocker);
    set_content_alignment(Anchor::Center);

    m_content = &add<Container>("modal-content", StackDirection::Vertical);
    m_content->set_size({px(360), fit()});
    m_content->set_spacing(12.0f);
}

void ModalOverlay::apply_theme_defaults(const Theme& theme) {
    LayerContainer::apply_theme_defaults(theme);

    configure_all_styles([theme](Style& style) {
        style.background_color(rgba(0.0f, 0.0f, 0.0f, 0.6f));
        style.border(BorderEdge{.size = 1.0f, .type = BorderType::Solid});
        style.border_color(theme.border_color);
    });

    m_content->configure_all_styles([&theme](Style& style) {
        style.padding({16.0f});
        style.background_color(theme.background_secondary_color);
        style.border_radius(8.0f);
    });
}
