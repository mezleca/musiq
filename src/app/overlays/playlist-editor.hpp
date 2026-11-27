#pragma once

#include "modal.hpp"
#include "../../youtube/schema/playlist.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

class MusiqButtonWidget;
class MusiqText;
class CoverPickerWidget;
class ThumbnailCache;
namespace ui {
    class DropdownWidget;
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
    using Delete = std::function<void(std::string)>;

    PlaylistEditorOverlay(ThumbnailCache& thumbnails, Save save, Delete delete_playlist);

    void show_create();
    void show_edit(const MusicPlaylist& playlist);
    void update_cover(std::string_view playlist_id, const std::vector<MusicThumbnail>& thumbnails);
    void show_delete(const std::string& id, const std::string& title);
    void finish();
    void fail(std::string message);

protected:
    void event(ui::UiEvent& event) override;

private:
    void show_form(const std::string& title, const std::string& description, MusicPlaylistPrivacy privacy);
    void pick_cover();
    void submit();
    void confirm_delete(bool confirm, const std::string& title = {});
    void set_busy(bool busy);
    void refresh_save_button();

    Save m_save;
    Delete m_delete_playlist;
    ui::Container* m_fields;
    CoverPickerWidget* m_cover;
    ui::TextInputWidget* m_title;
    ui::TextInputWidget* m_description;
    ui::DropdownWidget* m_visibility;
    MusiqText* m_status;
    MusiqButtonWidget* m_save_button;
    MusiqButtonWidget* m_delete_button;
    MusiqButtonWidget* m_delete_cancel;
    MusiqButtonWidget* m_delete_confirm;
    PlaylistEditorMode m_mode = PlaylistEditorMode::Save;
    std::string m_id;
    std::string m_privacy_value;
    std::string m_original_title;
    std::string m_original_description;
    MusicPlaylistPrivacy m_original_privacy = MusicPlaylistPrivacy::Private;
    std::filesystem::path m_cover_path;
    bool m_busy = false;
    bool m_can_delete = false;
    bool m_confirming_delete = false;
};
