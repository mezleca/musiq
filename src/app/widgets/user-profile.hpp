#pragma once

#include <imgui-ui/layout/container.hpp>
#include <functional>
#include <string_view>

namespace ui {
    class ImageWidget;
    class Texture;
} // namespace ui

class Button;
class Text;

class UserProfile final : public ui::Container {
public:
    explicit UserProfile(std::function<void()> sign_in);
    void set_busy(bool busy);
    void set_user(std::string_view name, ui::Texture* avatar);

private:
    Button* m_sign_in;
    ui::Container* m_account;
    ui::ImageWidget* m_avatar;
    Text* m_name;
};
