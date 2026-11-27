#include "context-menu.hpp"
#include "button.hpp"

#include <utility>

using namespace ui;

void MediaContextMenu::open_at(ImVec2 position, bool above) {
    m_position = position;
    m_above = above;
    ContextMenuWidget::open_at(position);
}

void MediaContextMenu::on_measure() {
    Container::on_measure();
    if (!is_open()) return;

    const auto area = viewport_work_area();
    if (!area.valid()) return;

    const auto size = layout().intrinsic_size();
    auto position = m_position;
    if (m_above) position.y -= size.y;
    position = clamp_position(area, size, position);
    const auto scale = dpi_scale();
    auto config = layout().config();
    config.placement.offset = {(position.x - area.min.x) / scale, (position.y - area.min.y) / scale};
    set_layout(config);
}

void add_menu_action(ContextMenuItems& items, std::string label, std::function<void()> run) {
    items.push_back(ContextMenuItem::action(std::move(label), [run = std::move(run)](ContextMenuWidget&) { run(); }));
}

class MenuTrigger final : public MusiqIconButtonWidget {
public:
    MenuTrigger(Widget& owner, OpenMenu open, ImVec2 anchor)
        : MusiqIconButtonWidget("icons/dots-vertical.svg", 24.0f, 16.0f), m_owner(owner), m_open(std::move(open)) {
        set_layout({
            .size = {px(24), px(22)},
            .placement = {.anchor = anchor, .origin = anchor, .offset = {}},
            .in_flow = false,
        });
    }

    void draw() override {
        if (m_owner.subtree_input_state().hovered || input_state().focused) {
            MusiqIconButtonWidget::draw();
        }
    }

    bool open_at(ImVec2 position) {
        return m_open(position);
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        MusiqIconButtonWidget::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) { style.background_color(theme.transparent); });
    }

    void click_event(UiEvent& event) override {
        open_at(layout().visual_rect().max);
        event.stop_propagation();
    }

private:
    Widget& m_owner;
    OpenMenu m_open;
};

void attach_context_menu(Widget& owner, OpenMenu open, Container* button_parent, ImVec2 anchor) {
    if (button_parent) {
        auto& trigger = button_parent->add<MenuTrigger>(owner, std::move(open), anchor);
        open = [&trigger](ImVec2 position) { return trigger.open_at(position); };
    }

    owner.on_event(EventType::ContextClick, [open = std::move(open)](UiEvent& event) {
        if (open(event.position)) event.stop_propagation();
    });
}
