#include "playlist-editor.hpp"
#include "../widgets/button.hpp"
#include "../widgets/text.hpp"
#include "../../utils/text.hpp"

#include <arbor/resources/texture-registry.hpp>
#include <arbor/runtime.hpp>
#include <arbor/surface.hpp>
#include <arbor/widgets/dropdown.hpp>
#include <arbor/widgets/image.hpp>
#include <arbor/widgets/text-input.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

using namespace ui;

class CoverPickerWidget final : public ImageWidget {
public:
    CoverPickerWidget() {
        set_type_name("CoverPickerWidget");
        set_size({px(160), px(160)});
        set_fit(ImageFit::Cover);
        set_input_mode(InputMode::Target);
    }

    std::function<void()> on_pick;

protected:
    void apply_theme_defaults(const Theme& theme) override {
        ImageWidget::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) {
            style.background_color(theme.background_tertiary_color).border_radius(theme.controls.rounding);
        });
    }

    void click_event(UiEvent& event) override {
        if (event.button == PointerButton::Left && on_pick) on_pick();
    }
};

PlaylistEditorOverlay::PlaylistEditorOverlay(Save save) : ModalOverlay("playlist-editor"), m_save(std::move(save)) {
    set_type_name("PlaylistEditorOverlay");

    auto& panel = content();
    panel.set_size({px(560), fit()});
    panel.set_spacing(20.0f);

    auto& fields = panel.add<Container>("fields", StackDirection::Horizontal);
    fields.set_size({grow(), fit()});
    fields.set_spacing(22.0f);

    auto& cover = fields.add<CoverPickerWidget>();
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
    m_visibility->on_select([this] {
        if (const auto parsed = playlist_privacy_from(m_privacy_value)) m_privacy = *parsed;
        refresh_save_button();
    });

    m_status = &panel.add<MusiqText>("");
    m_status->set_visible(false);

    m_save_button = &panel.add<MusiqButtonWidget>("Create");
    m_save_button->set_size({grow(), px(44)});
    m_save_button->on_click([this] { submit(); });
    refresh_save_button();
}

void PlaylistEditorOverlay::show_create() {
    m_mode = PlaylistEditorMode::Save;
    m_id.clear();
    show_form({}, {}, MusicPlaylistPrivacy::Private);
}

void PlaylistEditorOverlay::show_edit(const MusicPlaylist& playlist) {
    m_mode = PlaylistEditorMode::Edit;
    m_id = playlist.id;
    m_original_title = playlist.title;
    m_original_description = playlist.description;
    m_original_privacy = playlist.privacy;
    show_form(playlist.title, playlist.description, playlist.privacy);
}

void PlaylistEditorOverlay::show_form(const std::string& title, const std::string& description, MusicPlaylistPrivacy privacy) {
    m_privacy = privacy;
    m_privacy_value = std::string(playlist_privacy_value(m_privacy));
    m_cover_path.clear();
    m_cover->set_texture(TextureHandle{});

    m_title->set_value(title);
    m_description->set_value(description);
    m_visibility->select_value(m_privacy_value);

    set_busy(false);
    m_status->set_text({});
    m_status->set_visible(false);

    set_visible(true);
    m_title->focus();
}

void PlaylistEditorOverlay::set_busy(bool busy) {
    m_busy = busy;
    m_title->set_enabled(!busy);
    m_description->set_enabled(!busy);
    m_visibility->set_enabled(!busy);
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
    if (m_busy) return;

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
    m_cover->set_texture(std::move(texture));
    m_status->set_text({});
    m_status->set_visible(false);
    refresh_save_button();
}

void PlaylistEditorOverlay::submit() {
    if (m_busy) return;

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
            .privacy = m_privacy,
            .cover_path = m_cover_path,
        }
    );
}

void PlaylistEditorOverlay::refresh_save_button() {
    const bool named = !utils::text::trim(m_title->value()).empty();
    const bool changed = m_mode == PlaylistEditorMode::Save || !m_cover_path.empty() || m_title->value() != m_original_title ||
                         m_description->value() != m_original_description || m_privacy != m_original_privacy;
    m_save_button->set_text(m_mode == PlaylistEditorMode::Save ? "Create" : "Update");
    m_save_button->set_enabled(named && changed && !m_busy);
}

void PlaylistEditorOverlay::event(UiEvent& event) {
    if (!m_busy) {
        ModalOverlay::event(event);
    }
}
