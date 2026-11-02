#pragma once

#include <imgui-ui/layout/container.hpp>
#include <functional>
#include <string>
#include <string_view>

class UserProfileWidget;
class OptionsWidget;

class SideBarOverlay final : public ui::Container {
public:
    explicit SideBarOverlay(std::function<void()> sign_in, std::function<void(std::string_view)> navigate = {});

    UserProfileWidget& user_profile() {
        return *m_user_profile;
    }
    ui::Container& options() {
        return *m_options;
    }
    ui::Container& playlists() {
        return *m_playlists;
    }

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void add_option(std::string id, std::string text, std::string icon_path, bool selected = false);

    UserProfileWidget* m_user_profile;
    ui::Container* m_options;
    ui::Container* m_playlists;
    OptionsWidget* m_selected_option = nullptr;
    std::function<void(std::string_view)> m_navigate;
};
