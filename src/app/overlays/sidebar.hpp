#pragma once

#include <imgui-ui/layout/container.hpp>
#include <functional>

class UserProfileWidget;

class SideBarOverlay final : public ui::Container {
public:
    explicit SideBarOverlay(std::function<void()> sign_in);

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
    UserProfileWidget* m_user_profile;
    ui::Container* m_options;
    ui::Container* m_playlists;
};
