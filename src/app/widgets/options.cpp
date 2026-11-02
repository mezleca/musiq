#include "options.hpp"
#include "text.hpp"

#include <imgui-ui/resources/texture-registry.hpp>
#include <imgui-ui/runtime.hpp>
#include <imgui-ui/surface.hpp>
#include <imgui-ui/widgets/image.hpp>
#include <filesystem>
#include <utility>

using namespace ui;

OptionsWidget::OptionsWidget(std::string id, std::string text, std::string icon_path, bool selected)
    : Widget(std::move(id), StackDirection::Horizontal, "OptionsWidget"), m_icon_path(std::move(icon_path)),
      m_selected(selected) {
    set_size({grow(), px(40)});
    set_content_alignment({0.0f, 0.5f});
    set_spacing(12.0f);

    m_icon = &add<ImageWidget>();
    m_icon->set_size({px(24), px(24)});
    m_icon->set_fit(ImageFit::Contain);

    auto& label = add<MusiqText>(std::move(text), true);
    label.set_size({grow(), fit()});
    label.set_overflow(TextOverflow::Ellipsis);
}

void OptionsWidget::set_selected(bool selected) {
    m_selected = selected;
    const auto& theme = surface().theme();
    const auto background = selected ? theme.header_background_color : theme.transparent;
    style(StyleType::DEFAULT).background_color(background);
    style(StyleType::FOCUS).background_color(background);
}

void OptionsWidget::apply_theme_defaults(const Theme& theme) {
    Widget::apply_theme_defaults(theme);
    configure_all_styles([this, &theme](Style& style) {
        style.padding({8.0f, 4.0f, 8.0f, 4.0f});
        style.border_thickness(0.0f);
        style.border_radius(8.0f);
        style.background_color(m_selected ? theme.header_background_color : theme.transparent);
        style.cursor(ImGuiMouseCursor_Hand);
    });
    style(StyleType::HOVER).background_color(theme.controls.hover_color);
    style(StyleType::ACTIVE).background_color(theme.controls.active_color);

    auto& textures = surface().runtime().textures();
    m_icon->set_texture(textures.acquire(m_icon_path, std::filesystem::path(m_icon_path)));
    m_icon->configure_all_styles([&theme](Style& style) { style.color(theme.text_color); });
}
