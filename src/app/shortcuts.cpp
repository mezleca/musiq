#include "shortcuts.hpp"

void AppShortcuts::bind(ImGuiKey key, std::function<void()> action) {
    m_bindings.emplace_back(key, std::move(action));
}

void AppShortcuts::update() {
    for (const auto& [key, action] : m_bindings) {
        if (ImGui::IsKeyPressed(key, false)) action();
    }
}
