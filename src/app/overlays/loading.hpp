#pragma once

#include <imgui-ui/layout/layer-container.hpp>
#include <string>

class MusiqText;

class LoadingOverlay final : public ui::LayerContainer {
public:
    LoadingOverlay();

    void show(std::string status);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    MusiqText* m_status;
};
