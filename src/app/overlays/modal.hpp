#pragma once

#include <imgui-ui/layout/layer-container.hpp>
#include <string>

class ModalOverlay : public ui::LayerContainer {
public:
    explicit ModalOverlay(std::string id);

    ui::Container& content() {
        return *m_content;
    }

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    ui::Container* m_content;
};
