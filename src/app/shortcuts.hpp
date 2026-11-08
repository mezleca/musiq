#pragma once

#include <imgui.h>
#include <functional>
#include <utility>
#include <vector>

class AppShortcuts {
public:
    void bind(ImGuiKey key, std::function<void()> action);
    void update();

private:
    std::vector<std::pair<ImGuiKey, std::function<void()>>> m_bindings;
};
