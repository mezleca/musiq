#include "button.hpp"

#include <imgui-ui/surface.hpp>
#include <imgui-ui/widgets/image.hpp>
#include <imgui-ui/resources/texture-registry.hpp>
#include <imgui-ui/runtime.hpp>
#include <filesystem>
#include <utility>

using namespace ui;

MusiqButtonWidget::MusiqButtonWidget(std::string text, LayoutSize size) : ButtonWidget(std::move(text), size) {
    set_type_name("MusiqButtonWidget");
}

void MusiqButtonWidget::apply_theme_defaults(const Theme& theme) {
    ButtonWidget::apply_theme_defaults(theme);
    set_font(surface().get_font("Manrope Bold"), 14.0f);
}

MusiqIconButtonWidget::MusiqIconButtonWidget(std::string icon_path, float size)
    : MusiqButtonWidget("", {px(size), px(size)}), m_path(std::move(icon_path)) {
    set_type_name("MusiqIconButtonWidget");
    set_content_alignment(Anchor::Center);

    m_icon = &add<ImageWidget>();
    m_icon->set_size({px(24), px(24)});
}

void MusiqIconButtonWidget::apply_theme_defaults(const Theme& theme) {
    MusiqButtonWidget::apply_theme_defaults(theme);

    configure_all_styles([this, &theme](Style& style) {
        style.padding({})
            .background_color(m_selected ? theme.controls.hover_color : theme.transparent)
            .border_thickness(0.0f)
            .border_radius(8.0f);
    });

    style(StyleType::HOVER).background_color(m_selected ? theme.transparent : theme.controls.hover_color);
    m_icon->set_visible(!m_path.empty());

    if (!m_path.empty()) {
        m_icon->set_texture(surface().runtime().textures().acquire(m_path, std::filesystem::path(m_path)));
    }

    m_icon->configure_all_styles([&theme](Style& style) { style.color(theme.text_color); });

    m_icon->style(StyleType::DISABLED).color(theme.text_secondary_color);
}

void MusiqIconButtonWidget::set_selected(bool value) {
    if (m_selected == value) return;
    m_selected = value;
    apply_theme_defaults(surface().theme());
}

void MusiqIconButtonWidget::set_icon(std::string path) {
    if (m_path == path) {
        return;
    }

    m_path = std::move(path);
    m_icon->set_visible(!m_path.empty());

    if (!m_path.empty()) {
        m_icon->set_texture(surface().runtime().textures().acquire(m_path, std::filesystem::path(m_path)));
    }
}
