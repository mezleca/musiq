#pragma once

#include <arbor/layout/container.hpp>
#include <functional>
#include <string>
#include <vector>

struct MusicSuggestions;
class SearchInputWidget;
class SuggestionButtonWidget;

namespace ui {
    class LayerContainer;
}

class SearchBarWidget final : public ui::Container {
public:
    SearchBarWidget(std::function<void(std::string)> submit, std::function<void(std::string)> edit);
    void set_suggestions(const MusicSuggestions& suggestions);
    void set_query(std::string query);

private:
    void hide_suggestions();
    void move_selection(int direction);
    void replace_query(std::string query);
    void confirm_query();

    std::function<void(std::string)> m_submit;
    std::function<void(std::string)> m_edit;
    SearchInputWidget* m_input;
    ui::LayerContainer* m_suggestions;
    std::vector<SuggestionButtonWidget*> m_choice_buttons;
    int m_selected = -1;
    bool m_restoring_query = false;
};
