#pragma once

#include <imgui-ui/layout/resizable-container.hpp>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class OptionsWidget;
class ThumbnailCache;
struct MusicItem;

class SideBarOverlay final : public ui::ResizableContainer {
public:
    explicit SideBarOverlay(
        ThumbnailCache& thumbnails, std::function<void(std::string_view)> navigate = {},
        std::function<void(MusicItem)> open_playlist = {}
    );
    void select(std::string_view id);
    void set_playlists(const std::vector<MusicItem>& playlists);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override;

private:
    void add_option(std::string id, std::string text, std::string icon_path, bool selected = false);

    ui::Container* m_options;
    ui::Container* m_playlists;
    ThumbnailCache& m_thumbnails;
    OptionsWidget* m_selected_option = nullptr;
    std::function<void(std::string_view)> m_navigate;
    std::function<void(MusicItem)> m_open_playlist;
    std::string m_selected_id = "home";
};
