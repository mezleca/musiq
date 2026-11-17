#pragma once

#include "modal.hpp"
#include "../../youtube/schema/playlist.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

class MusiqButtonWidget;
class MusiqText;
namespace ui {
    class DropdownWidget;
    class ImageWidget;
    class TextInputWidget;
} // namespace ui

enum class PlaylistEditorMode : uint8_t {
    Save,
    Edit,
};

struct PlaylistDraft {
    PlaylistEditorMode mode = PlaylistEditorMode::Save;
    std::string id;
    std::string title;
    std::string description;
    MusicPlaylistPrivacy privacy = MusicPlaylistPrivacy::Private;
    std::filesystem::path cover_path;
};

class PlaylistEditorOverlay final : public ModalOverlay {
public:
    using Save = std::function<void(PlaylistDraft)>;

    explicit PlaylistEditorOverlay(Save save);

    void show_create();
    void show_edit(const MusicPlaylist& playlist);
    void set_busy(bool busy);
    void finish();
    void fail(std::string message);

protected:
    void event(ui::UiEvent& event) override;

private:
    void pick_cover();
    void submit();
    void refresh_save_button();

    Save m_save;
    ui::ImageWidget* m_cover;
    ui::TextInputWidget* m_title;
    ui::TextInputWidget* m_description;
    ui::DropdownWidget* m_visibility;
    MusiqText* m_status;
    MusiqButtonWidget* m_save_button;
    PlaylistEditorMode m_mode = PlaylistEditorMode::Save;
    std::string m_id;
    std::string m_privacy_value;
    MusicPlaylistPrivacy m_privacy = MusicPlaylistPrivacy::Private;
    std::string m_original_title;
    std::string m_original_description;
    MusicPlaylistPrivacy m_original_privacy = MusicPlaylistPrivacy::Private;
    std::filesystem::path m_cover_path;
    bool m_busy = false;
};
