#pragma once

#include <imgui-ui/layout/resizable-container.hpp>
#include <functional>
#include <string>
#include <string_view>

class OptionsWidget;

class SideBarOverlay final : public ui::ResizableContainer {
public:
    explicit SideBarOverlay(std::function<void(std::string_view)> navigate = {});
    void select(std::string_view id);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void add_option(std::string id, std::string text, std::string icon_path, bool selected = false);

    ui::Container* m_options;
    ui::Container* m_playlists;
    OptionsWidget* m_selected_option = nullptr;
    std::function<void(std::string_view)> m_navigate;
};
