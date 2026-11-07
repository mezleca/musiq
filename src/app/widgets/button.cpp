#include "button.hpp"

#include <imgui-ui/surface.hpp>
#include <imgui-ui/widgets/image.hpp>
#include <imgui-ui/resources/texture-registry.hpp>
#include <imgui-ui/runtime.hpp>
#include <filesystem>
#include <utility>

MusiqButtonWidget::MusiqButtonWidget(std::string text, ui::LayoutSize size) : ui::ButtonWidget(std::move(text), size) {
    set_type_name("MusiqButtonWidget");
}

void MusiqButtonWidget::apply_theme_defaults(const ui::Theme& theme) {
    ui::ButtonWidget::apply_theme_defaults(theme);

    set_font(surface().get_font("Manrope Bold"), 14.0f);
}

MusiqIconButtonWidget::MusiqIconButtonWidget(std::string icon_path, float size)
    : MusiqButtonWidget("", {ui::px(size), ui::px(size)}), m_path(std::move(icon_path)) {
    set_type_name("MusiqIconButtonWidget");
    set_content_alignment(ui::Anchor::Center);
    m_icon = &add<ui::ImageWidget>();
    m_icon->set_size({ui::px(24), ui::px(24)});
}

void MusiqIconButtonWidget::apply_theme_defaults(const ui::Theme& theme) {
    MusiqButtonWidget::apply_theme_defaults(theme);
    configure_all_styles([&theme](ui::Style& style) {
        style.padding({}).background_color(theme.transparent).border_thickness(0.0f).border_radius(8.0f);
    });
    style(ui::StyleType::HOVER).background_color(theme.controls.hover_color);
    m_icon->set_visible(!m_path.empty());
    if (!m_path.empty()) m_icon->set_texture(surface().runtime().textures().acquire(m_path, std::filesystem::path(m_path)));
    m_icon->configure_all_styles([&theme](ui::Style& style) { style.color(theme.text_color); });
    m_icon->style(ui::StyleType::DISABLED).color(theme.text_secondary_color);
}

void MusiqIconButtonWidget::set_icon(std::string path) {
    if (m_path == path) return;
    m_path = std::move(path);
    m_icon->set_visible(!m_path.empty());
    if (!m_path.empty()) {
        m_icon->set_texture(surface().runtime().textures().acquire(m_path, std::filesystem::path(m_path)));
    }
}
