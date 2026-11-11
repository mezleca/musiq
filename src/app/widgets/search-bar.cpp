#include "search-bar.hpp"
#include "button.hpp"
#include "../../utils/path.hpp"
#include "../../utils/text.hpp"
#include "../../youtube/schema/catalog.hpp"

#include <imgui-ui/layout/layer-container.hpp>
#include <imgui-ui/resources/texture-registry.hpp>
#include <imgui-ui/runtime.hpp>
#include <imgui-ui/surface.hpp>
#include <imgui-ui/widgets/text-input.hpp>
#include <imgui-ui/widgets/image.hpp>
#include <algorithm>
#include <utility>

using namespace ui;

class SearchInputWidget final : public TextInputWidget {
public:
    SearchInputWidget() : TextInputWidget({}, {}, LabelPlacement::Inline, "search-query") {
        set_size({grow(), px(48)});
        set_hint("Search songs, albums, artists");
        set_flags(ImGuiInputTextFlags_EscapeClearsAll);
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        TextInputWidget::apply_theme_defaults(theme);
        configure_all_styles([](Style& style) { style.font_size(16.0f); });

        frame().set_size({grow(), grow()});
        frame().set_spacing(12.0f);
        frame().configure_all_styles([&theme](Style& style) {
            style.padding({8.0f, 6.0f, 8.0f, 6.0f});
            style.background_color(theme.background_secondary_color);
            style.border_radius(theme.controls.rounding);
            style.border_thickness(1.0f);
            style.border_color(theme.border_color);
        });

        frame().style(StyleType::FOCUS).border_color(theme.accent_color);
        frame().style(StyleType::ACTIVE).border_color(theme.accent_color);
        icon().style(StyleType::HOVER).color(theme.text_secondary_color);
        set_icon(surface().runtime().textures().acquire("search-icon", utils::path::from_resources("icons/search.svg")));
    }
};

class SearchSuggestionsOverlay final : public LayerContainer {
public:
    SearchSuggestionsOverlay() : LayerContainer("search-suggestions", LayerMode::Window) {
        set_type_name("SearchSuggestionsOverlay");
        set_layout(
            {.size = {grow(), fit()},
             .placement = {.anchor = Anchor::BottomLeft, .origin = Anchor::TopLeft, .offset = {0.0f, 4.0f}},
             .in_flow = false}
        );
        set_spacing(2.0f);
        set_visible(false);
    }

protected:
    ImGuiWindowFlags window_flags() const override {
        // suggestions can refresh while typing, so their window must leave focus in the input
        return LayerContainer::window_flags() | ImGuiWindowFlags_NoFocusOnAppearing;
    }

    void apply_theme_defaults(const Theme& theme) override {
        LayerContainer::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) {
            style.padding({8.0f});
            style.background_color(theme.background_secondary_color);
            style.border_radius(theme.controls.rounding);
            style.border_thickness(1.0f);
            style.border_color(theme.border_color);
        });
    }
};

class SuggestionButtonWidget final : public MusiqButtonWidget {
public:
    explicit SuggestionButtonWidget(std::string text) : MusiqButtonWidget(std::move(text), {grow(), px(40)}) {
        set_text_alignment({0.0f, 0.5f});
        set_text_overflow(TextOverflow::Ellipsis);
    }

    void set_selected(bool selected) {
        m_selected = selected;
        const auto& theme = surface().theme();
        const auto background = selected ? theme.header_background_color : theme.transparent;
        configure_all_styles([background](Style& style) { style.background_color(background); });
        style(StyleType::HOVER).background_color(theme.controls.hover_color);
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        MusiqButtonWidget::apply_theme_defaults(theme);
        configure_all_styles([](Style& style) {
            style.font_size(16.0f);
            style.padding({8.0f, 12.0f, 8.0f, 12.0f});
            style.border_thickness(0.0f);
            style.border_radius(6.0f);
        });
        set_selected(m_selected);
    }

private:
    bool m_selected = false;
};

SearchBarWidget::SearchBarWidget(std::function<void(std::string)> submit, std::function<void(std::string)> edit)
    : Container("search-bar", StackDirection::Vertical), m_submit(std::move(submit)), m_edit(std::move(edit)) {
    set_type_name("SearchBarWidget");
    set_size({{px(0), px(800)}, fit()});
    set_content_alignment(Anchor::TopCenter);

    m_input = &add<SearchInputWidget>();

    auto& clear = m_input->trailing().add<MusiqIconButtonWidget>("icons/close.svg");
    clear.set_id("clear-search");
    clear.set_tooltip("Clear search");
    clear.on_click([this] {
        replace_query({});
        m_input->focus();
    });

    m_input->trailing().set_visible(false);
    m_input->on_change([this] {
        hide_suggestions();
        m_input->trailing().set_visible(!m_input->value().empty());
        if (!m_restoring_query) m_edit(m_input->value());
    });

    m_input->on_submit([this] { confirm_query(); });
    m_input->on_event(EventType::KeyDown, [this](UiEvent& event) {
        if (event.key == Key::Up || event.key == Key::Down) {
            move_selection(event.key == Key::Up ? -1 : 1);
        } else if (event.key == Key::Enter && m_selected >= 0) {
            replace_query(m_choice_buttons[m_selected]->text());
        } else {
            return;
        }

        event.block_native_input();
        event.stop_propagation();
    });

    m_suggestions = &m_input->add<SearchSuggestionsOverlay>();
}

void SearchBarWidget::hide_suggestions() {
    // keep rows alive until the next response so a suggestion's click callback can finish
    m_suggestions->set_visible(false);
    m_choice_buttons.clear();
    m_selected = -1;
}

void SearchBarWidget::set_suggestions(const MusicSuggestions& suggestions) {
    hide_suggestions();
    m_suggestions->clear();

    for (std::size_t index = 0; index < std::min<std::size_t>(suggestions.queries.size(), 6); ++index) {
        const auto& query = suggestions.queries[index].query;
        auto& button = m_suggestions->add<SuggestionButtonWidget>(query);
        button.set_id("suggestion-" + std::to_string(index));
        button.on_click([this, query] {
            replace_query(query);
            confirm_query();
        });
        m_choice_buttons.push_back(&button);
    }

    m_suggestions->set_visible(visible() && !m_input->value().empty() && !m_choice_buttons.empty());
}

void SearchBarWidget::move_selection(int direction) {
    if (m_choice_buttons.empty()) {
        return;
    }

    const int count = static_cast<int>(m_choice_buttons.size());
    if (m_selected < 0) {
        m_selected = direction > 0 ? 0 : count - 1;
    } else {
        m_choice_buttons[m_selected]->set_selected(false);
        m_selected = (m_selected + direction + count) % count;
    }

    m_choice_buttons[m_selected]->set_selected(true);
}

void SearchBarWidget::replace_query(std::string query) {
    set_query(std::move(query));
    m_edit(m_input->value());
}

void SearchBarWidget::confirm_query() {
    hide_suggestions();

    const auto query = utils::text::trim(m_input->value());
    if (query.empty()) return;

    m_submit(std::string(query));
}

void SearchBarWidget::set_query(std::string query) {
    // restore route text without starting a new suggestion request
    m_restoring_query = true;
    m_input->set_value(std::move(query));
    m_restoring_query = false;
    hide_suggestions();
}
