#include "playlist-editor.hpp"
#include "../widgets/button.hpp"
#include "../widgets/media-cover.hpp"
#include "../widgets/text.hpp"
#include "../../utils/path.hpp"
#include "../../utils/text.hpp"

#include <arbor/resources/texture-registry.hpp>
#include <arbor/runtime.hpp>
#include <arbor/surface.hpp>
#include <arbor/widgets/dropdown.hpp>
#include <arbor/widgets/text-input.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

using namespace ui;

class CoverPickerWidget final : public MediaCoverWidget {
public:
    explicit CoverPickerWidget(ThumbnailCache& thumbnails) : MediaCoverWidget(thumbnails, 160.0f) {
        set_type_name("CoverPickerWidget");
        set_input_mode(InputMode::Target);
        set_content_alignment(Anchor::Center);
        m_placeholder = &add<ImageWidget>();
        m_placeholder->set_size({px(40), px(40)});
    }

    void show_existing(std::span<const MusicThumbnail> thumbnails) {
        if (!m_show_remote) set_texture(nullptr);
        m_show_remote = !thumbnails.empty();
        m_placeholder->set_visible(!m_show_remote);
        set_thumbnails(thumbnails);
    }

    void show_preview(TextureHandle texture) {
        m_show_remote = false;
        m_placeholder->set_visible(false);
        set_thumbnails({});
        set_texture(std::move(texture));
    }

    std::function<void()> on_pick;

protected:
    void apply_theme_defaults(const Theme& theme) override {
        MediaCoverWidget::apply_theme_defaults(theme);

        configure_all_styles([&theme](Style& style) {
            style.background_color(theme.background_tertiary_color)
                .border_radius(theme.controls.rounding)
                .cursor(ImGuiMouseCursor_Hand);
        });

        style(StyleType::DISABLED).cursor(ImGuiMouseCursor_Arrow);

        auto& textures = surface().runtime().textures();
        m_placeholder->set_texture(textures.acquire("icons/image-plus.svg", utils::path::from_resources("icons/image-plus.svg")));
    }

    void click_event(UiEvent& event) override {
        if (event.button == PointerButton::Left && on_pick) on_pick();
    }

    void draw_before() override {
        if (m_show_remote) MediaCoverWidget::draw_before();
    }

private:
    ImageWidget* m_placeholder;
    bool m_show_remote = false;
};

PlaylistEditorOverlay::PlaylistEditorOverlay(ThumbnailCache& thumbnails, Save save, Delete delete_playlist)
    : ModalOverlay("playlist-editor"), m_save(std::move(save)), m_delete_playlist(std::move(delete_playlist)) {
    set_type_name("PlaylistEditorOverlay");

    auto& panel = content();
    panel.set_size({px(560), fit()});
    panel.set_spacing(20.0f);

    auto& fields = panel.add<Container>("fields", StackDirection::Horizontal);
    fields.set_size({grow(), fit()});
    fields.set_spacing(22.0f);

    auto& cover = fields.add<CoverPickerWidget>(thumbnails);
    cover.on_pick = [this] { pick_cover(); };
    m_cover = &cover;

    auto& details = fields.add<Container>("details", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(16.0f);

    m_title = &details.add<TextInputWidget>();
    m_title->set_id("playlist-title");
    m_title->set_hint("Playlist name");
    m_title->set_size({grow(), fit()});
    m_title->on_change([this] { refresh_save_button(); });

    m_description = &details.add<TextInputWidget>();
    m_description->set_id("playlist-description");
    m_description->set_hint("Description");
    m_description->set_multiline();
    m_description->set_rows(3, 6);
    m_description->set_size({grow(), fit()});
    m_description->on_change([this] { refresh_save_button(); });

    m_visibility = &details.add<DropdownWidget>(
        m_privacy_value,
        std::vector<DropdownOption>{
            {.label = "Public", .value = "PUBLIC", .description = {}},
            {.label = "Unlisted", .value = "UNLISTED", .description = {}},
            {.label = "Private", .value = "PRIVATE", .description = {}},
        },
        "Visibility", LabelPlacement::Above, "playlist-visibility"
    );
    m_visibility->set_size({grow(), fit()});
    m_visibility->on_select([this] { refresh_save_button(); });

    m_status = &panel.add<MusiqText>("");
    m_status->set_visible(false);

    auto& actions = panel.add<Container>("playlist-actions", StackDirection::Horizontal);
    actions.set_size({grow(), fit()});
    actions.set_content_alignment(Anchor::CenterRight);
    actions.set_spacing(10.0f);

    m_delete_button = &actions.add<MusiqDangerButtonWidget>("Delete playlist", LayoutSize{fit(), px(40)});
    m_delete_button->set_id("playlist-delete");
    m_delete_button->on_click([this] {
        if (m_can_delete && !m_busy && !m_confirming_delete) confirm_delete(true);
    });

    m_save_button = &actions.add<MusiqButtonWidget>("Create", LayoutSize{fit(), px(40)});
    m_save_button->on_click([this] { submit(); });

    m_delete_cancel = &actions.add<MusiqButtonWidget>("Cancel", LayoutSize{fit(), px(40)});
    m_delete_cancel->set_id("playlist-delete-cancel");
    m_delete_cancel->on_click([this] {
        if (!m_busy) confirm_delete(false);
    });
    m_delete_confirm = &actions.add<MusiqDangerButtonWidget>("Delete permanently", LayoutSize{fit(), px(40)});
    m_delete_confirm->set_id("playlist-delete-confirm");
    m_delete_confirm->on_click([this] {
        if (m_busy || !m_confirming_delete || !m_can_delete) return;

        set_busy(true);
        m_status->set_text("Deleting playlist...");
        m_delete_playlist(m_id);
    });
    confirm_delete(false);
}

void PlaylistEditorOverlay::show_create() {
    m_mode = PlaylistEditorMode::Save;
    m_id.clear();
    m_can_delete = false;
    show_form({}, {}, MusicPlaylistPrivacy::Private);
    m_cover->show_existing({});
}

void PlaylistEditorOverlay::show_edit(const MusicPlaylist& playlist) {
    m_mode = PlaylistEditorMode::Edit;
    m_id = playlist.id;
    m_can_delete = playlist.owned && !m_id.empty() && static_cast<bool>(m_delete_playlist);
    m_original_title = playlist.title;
    m_original_description = playlist.description;
    m_original_privacy = playlist.privacy;
    show_form(playlist.title, playlist.description, playlist.privacy);
    m_cover->show_existing(playlist.thumbnails);
}

void PlaylistEditorOverlay::show_delete(const MusicPlaylist& playlist) {
    if (m_busy || !playlist.owned || playlist.id.empty()) return;
    show_edit(playlist);
    confirm_delete(true);
}

void PlaylistEditorOverlay::show_form(const std::string& title, const std::string& description, MusicPlaylistPrivacy privacy) {
    m_privacy_value = playlist_privacy_value(privacy);
    m_cover_path.clear();

    m_title->set_value(title);
    m_description->set_value(description);
    m_visibility->select_value(m_privacy_value);
    m_save_button->set_text(m_mode == PlaylistEditorMode::Save ? "Create" : "Update");

    confirm_delete(false);

    set_visible(true);
    m_title->focus();
}

void PlaylistEditorOverlay::set_busy(bool busy) {
    m_busy = busy;
    const bool editing = !busy && !m_confirming_delete;
    m_cover->set_enabled(editing);
    m_title->set_enabled(editing);
    m_description->set_enabled(editing);
    m_visibility->set_enabled(editing);
    m_delete_button->set_enabled(editing);
    m_delete_cancel->set_enabled(!busy);
    m_delete_confirm->set_enabled(!busy);
    refresh_save_button();
}

void PlaylistEditorOverlay::finish() {
    set_busy(false);
    set_visible(false);
}

void PlaylistEditorOverlay::fail(std::string message) {
    set_busy(false);
    m_status->set_text(std::move(message));
    m_status->set_visible(!m_status->empty());
}

void PlaylistEditorOverlay::pick_cover() {
    if (m_busy || m_confirming_delete) return;

    FileDialogOptions options;
    options.filters = {{"Images", "png,jpg,jpeg"}};
    options.title = "Choose playlist cover";

    const auto result = surface().file_dialog().show(FileDialogOperation::OpenFile, options);
    if (!result.accepted() || result.paths.empty()) return;

    const auto selected = result.paths.front();
    auto extension = selected.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (extension != ".jpg" && extension != ".jpeg" && extension != ".png") {
        m_status->set_text("Choose a JPEG or PNG image.");
        m_status->set_visible(true);
        return;
    }

    std::error_code file_error;
    const auto size = std::filesystem::file_size(selected, file_error);
    if (file_error || size == 0 || size > 8 * 1024 * 1024) {
        m_status->set_text("Choose an image smaller than 8 MiB.");
        m_status->set_visible(true);
        return;
    }

    m_cover_path = selected;
    auto texture = surface().runtime().textures().acquire(
        "playlist-cover:" + m_cover_path.string(), m_cover_path, TextureRetention::Transient
    );
    m_cover->show_preview(std::move(texture));
    m_status->set_text({});
    m_status->set_visible(false);
    refresh_save_button();
}

void PlaylistEditorOverlay::submit() {
    if (m_busy || m_confirming_delete) return;

    const auto title = utils::text::trim(m_title->value());
    if (title.empty()) {
        m_status->set_text("Enter a playlist name.");
        m_status->set_visible(true);
        return;
    }

    set_busy(true);
    m_status->set_text(m_mode == PlaylistEditorMode::Save ? "Creating playlist..." : "Saving changes...");
    m_status->set_visible(true);
    m_save(
        PlaylistDraft{
            .mode = m_mode,
            .id = m_id,
            .title = std::string(title),
            .description = std::string(utils::text::trim(m_description->value())),
            .privacy = playlist_privacy_from(m_privacy_value).value_or(m_original_privacy),
            .cover_path = m_cover_path,
        }
    );
}

void PlaylistEditorOverlay::confirm_delete(bool confirm) {
    m_confirming_delete = confirm;
    m_save_button->set_visible(!confirm);
    m_delete_button->set_visible(m_can_delete && !confirm);
    m_delete_cancel->set_visible(confirm);
    m_delete_confirm->set_visible(confirm);
    m_status->set_text(confirm ? "Permanently delete this playlist? This action cannot be undone." : "");
    m_status->set_visible(confirm);
    set_busy(false);
}

void PlaylistEditorOverlay::refresh_save_button() {
    const bool named = !utils::text::trim(m_title->value()).empty();
    const bool changed = m_mode == PlaylistEditorMode::Save || !m_cover_path.empty() || m_title->value() != m_original_title ||
                         m_description->value() != m_original_description ||
                         m_privacy_value != playlist_privacy_value(m_original_privacy);
    m_save_button->set_enabled(named && changed && !m_busy);
}

void PlaylistEditorOverlay::event(UiEvent& event) {
    if (!m_busy) {
        ModalOverlay::event(event);
    }
}
