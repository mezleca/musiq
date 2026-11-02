#include "user-profile.hpp"
#include "button.hpp"
#include "text.hpp"

#include <imgui-ui/widgets/image.hpp>
#include <imgui-ui/style/theme.hpp>
#include <utility>

using namespace ui;

class AuthButtonWidget final : public MusiqButtonWidget {
public:
    AuthButtonWidget() : MusiqButtonWidget("Sign in", {grow(), fit()}) {
        set_type_name("AuthButtonWidget");
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        MusiqButtonWidget::apply_theme_defaults(theme);

        configure_all_styles([](Style& style) {
            style.background_color(rgba(0.0f, 0.0f, 0.0f, 0.0f));
            style.border_thickness(0.0f);
        });

        style(StyleType::HOVER).background_color(theme.accent_color);
    }
};

class AccountWidget final : public Container {
public:
    AccountWidget() : Container("account", StackDirection::Horizontal) {
        set_type_name("AccountWidget");
        set_size({grow(), fit()});
        set_content_alignment(Anchor::Center);
        set_spacing(12.0f);
        set_visible(false);

        m_avatar = &add<ImageWidget>();
        m_avatar->set_size({px(40), px(40)});
        m_avatar->set_fit(ImageFit::Cover);
        m_avatar->configure_all_styles([](Style& style) { style.border_radius(20.0f); });

        m_name = &add<MusiqText>("", true);
        m_name->set_size({grow(), fit()});
        m_name->set_overflow(TextOverflow::Ellipsis);
    }

    void update_data(std::string_view name, Texture* texture) {
        m_name->set_text(std::string(name));
        m_avatar->set_texture(texture);
    }

private:
    ImageWidget* m_avatar;
    MusiqText* m_name;
};

UserProfileWidget::UserProfileWidget(std::function<void()> sign_in) : Container("user-profile", StackDirection::Vertical) {
    set_type_name("UserProfileWidget");
    set_size({grow(), fit()});

    m_sign_in = &add<AuthButtonWidget>();
    m_sign_in->on_click([this, sign_in = std::move(sign_in)] {
        set_busy(true);
        sign_in();
    });

    m_account = &add<AccountWidget>();
}

void UserProfileWidget::apply_theme_defaults(const ui::Theme& theme) {
    Container::apply_theme_defaults(theme);

    TransitionSpec transition(0.10f);

    configure_all_styles([theme, transition](Style& style) {
        style.background_color(theme.background_color, transition);
        style.border(BorderEdge{.size = 1.0f, .type = BorderType::Solid});
        style.border_color(theme.background_secondary_color, transition);
        style.padding({8.0f, 4.0f, 8.0f, 4.0f});
    });

    auto apply_active_border = [this, theme, transition](StyleType type) {
        style(type).border_color(theme.border_color, transition).cursor(ImGuiMouseCursor_Hand);
    };

    apply_active_border(StyleType::HOVER);
    apply_active_border(StyleType::ACTIVE);
}

void UserProfileWidget::set_busy(bool busy) {
    m_sign_in->set_enabled(!busy);
    m_sign_in->set_text(busy ? "Signing in..." : "Sign in");
}

void UserProfileWidget::set_user(std::string_view name, Texture* avatar) {
    m_account->update_data(name, avatar);
    m_sign_in->set_visible(false);
    m_account->set_visible(true);
}
