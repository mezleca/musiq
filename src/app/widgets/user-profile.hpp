#pragma once

#include <arbor/layout/container.hpp>
#include <functional>
#include <string_view>

namespace ui {
    class Texture;
} // namespace ui

class AuthButtonWidget;
class AccountWidget;

class UserProfileWidget final : public ui::Container {
public:
    UserProfileWidget(std::function<void()> sign_in, std::function<void()> choose_account);
    void apply_theme_defaults(const ui::Theme& theme) override;

    void set_busy(bool busy);
    void set_user(std::string_view name, ui::Texture* avatar);
    void clear_user();

private:
    AuthButtonWidget* m_sign_in;
    AccountWidget* m_account;
};
