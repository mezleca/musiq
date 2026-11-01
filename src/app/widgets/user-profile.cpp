#include "user-profile.hpp"
#include "button.hpp"
#include "text.hpp"

#include <imgui-ui/widgets/image.hpp>
#include <utility>

using namespace ui;

UserProfile::UserProfile(std::function<void()> sign_in) : Container("user-profile", StackDirection::Vertical) {
    set_type_name("UserProfile");
    set_size({grow(), fit()});
    m_sign_in = &add<Button>("Sign in", LayoutSize{grow(), fit()});
    m_sign_in->on_click([this, sign_in = std::move(sign_in)] {
        set_busy(true);
        sign_in();
    });

    m_account = &add<Container>("account", StackDirection::Horizontal);
    m_account->set_size({grow(), fit()});
    m_account->set_content_alignment(Anchor::Center);
    m_account->set_spacing(12.0F);
    m_account->set_visible(false);

    m_avatar = &m_account->add<ImageWidget>();
    m_avatar->set_size({px(40), px(40)});
    m_avatar->set_fit(ImageFit::Cover);
    m_avatar->configure_all_styles([](Style& style) { style.border_radius(20.0F); });
    m_name = &m_account->add<Text>("", true);
    m_name->set_size({grow(), fit()});
    m_name->set_overflow(TextOverflow::Ellipsis);
}

void UserProfile::set_busy(bool busy) {
    m_sign_in->set_enabled(!busy);
    m_sign_in->set_text(busy ? "Signing in..." : "Sign in");
}

void UserProfile::set_user(std::string_view name, Texture* avatar) {
    m_name->set_text(std::string(name));
    m_avatar->set_texture(avatar);
    m_sign_in->set_visible(false);
    m_account->set_visible(true);
}
