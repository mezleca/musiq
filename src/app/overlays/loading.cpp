#include "loading.hpp"
#include "../widgets/spinner.hpp"
#include "../widgets/text.hpp"

#include <utility>

using namespace ui;

LoadingOverlay::LoadingOverlay() : LayerContainer("loading") {
    set_type_name("LoadingOverlay");
    set_input_mode(InputMode::Blocker);
    set_content_alignment(Anchor::Center);
    set_spacing(12.0f);
    add<SpinnerWidget>();
    m_status = &add<MusiqText>("");
    hide();
}

void LoadingOverlay::show() {
    set_visible(true);
}

void LoadingOverlay::hide() {
    set_visible(false);
}

void LoadingOverlay::update(std::string status) {
    if (!visible()) return;
    m_status->set_text(std::move(status));
}

void LoadingOverlay::apply_theme_defaults(const Theme& theme) {
    LayerContainer::apply_theme_defaults(theme);
    configure_all_styles([](Style& style) { style.background_color(rgba(0.0f, 0.0f, 0.0f, 0.45f)); });
}
