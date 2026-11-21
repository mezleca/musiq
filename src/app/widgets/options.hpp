#pragma once

#include "../../youtube/schema/common.hpp"
#include <arbor/widgets/widget.hpp>
#include <string>

namespace ui {
    class ImageWidget;
    class TextWidget;
} // namespace ui
class ThumbnailCache;
class MediaCoverWidget;
class MusiqText;

class OptionsWidget final : public ui::Widget {
public:
    OptionsWidget(std::string id, std::string text, std::string icon_path, bool selected = false);
    OptionsWidget(const MusicItem& playlist, ThumbnailCache& thumbnails);
    void set_playlist(const MusicItem& playlist);
    const MusicItem& playlist() const {
        return m_playlist;
    }
    void set_selected(bool selected);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    explicit OptionsWidget(std::string id);
    void apply_selection();

    ui::ImageWidget* m_icon = nullptr;
    MediaCoverWidget* m_cover = nullptr;
    MusiqText* m_title = nullptr;
    ui::TextWidget* m_creator = nullptr;
    MusicItem m_playlist;
    std::string m_icon_path;
    bool m_selected = false;
};
