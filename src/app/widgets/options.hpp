#pragma once

#include <arbor/widgets/widget.hpp>
#include <string>

namespace ui {
    class ImageWidget;
} // namespace ui
struct MusicItem;
class ThumbnailCache;

class OptionsWidget final : public ui::Widget {
public:
    OptionsWidget(std::string id, std::string text, std::string icon_path, bool selected = false);
    OptionsWidget(const MusicItem& playlist, ThumbnailCache& thumbnails);
    void set_selected(bool selected);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    explicit OptionsWidget(std::string id);
    void apply_selection();

    ui::ImageWidget* m_icon = nullptr;
    std::string m_icon_path;
    bool m_selected = false;
};
