#pragma once

#include <arbor/widgets/context-menu.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

struct MusicItem;

struct MediaMenuContext {
    std::string_view playlist_id;
    std::uint64_t queue_entry_id = 0;
    bool library_entry = false;
};

using MediaMenu = std::function<bool(const MusicItem&, MediaMenuContext, ImVec2)>;
using OpenMenu = std::function<bool(ImVec2)>;

void add_menu_action(ui::ContextMenuItems& items, std::string label, std::function<void()> run);

void attach_context_menu(
    ui::Widget& owner, OpenMenu open, ui::Container* button_parent = nullptr, ImVec2 anchor = ui::Anchor::TopRight
);
