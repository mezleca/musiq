#pragma once

#include <imgui-ui/layout/layer-container.hpp>
#include <string>

class Text;

class LoadingOverlay final : public ui::LayerContainer {
public:
    LoadingOverlay();

    void show();
    void hide();
    void update(std::string status);
    using ui::LayerContainer::update;

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    Text* m_status;
};
