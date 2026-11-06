#include "catalog-card.hpp"
#include "text.hpp"
#include "spinner.hpp"
#include "../resources/thumbnails.hpp"
#include "../../youtube/schema/common.hpp"
#include "../../utils/log.hpp"

#include <imgui-ui/widgets/image.hpp>
#include <imgui-ui/resources/texture-registry.hpp>
#include <imgui-ui/runtime.hpp>
#include <imgui-ui/surface.hpp>
#include <algorithm>

using namespace ui;

class CatalogCoverWidget final : public ImageWidget {
public:
    CatalogCoverWidget(const MusicItem& item, ThumbnailCache& thumbnails, float size) {
        set_type_name("CatalogCoverWidget");
        set_id("cover");
        set_size({px(size), px(size)});
        set_fit(ImageFit::Cover);

        if (!item.thumbnails.empty()) {
            const auto& image = *std::ranges::max_element(item.thumbnails, {}, &MusicThumbnail::width);
            if (!image.url.empty()) {
                m_url = image.url;
                m_download = thumbnails.request(m_url);
            }
        }

        m_spinner = &add<SpinnerWidget>();
        m_spinner->set_layout(
            {.size = {px(24), px(24)},
             .placement = {.anchor = Anchor::Center, .origin = Anchor::Center, .offset = {}},
             .in_flow = false}
        );
        m_spinner->set_visible(m_download.valid());
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        ImageWidget::apply_theme_defaults(theme);
        configure_all_styles([](Style& style) { style.color(rgba(255, 255, 255, 255)).border_radius(6.0f); });

        if (!m_loaded && !m_download.valid()) {
            set_texture(
                surface().runtime().textures().acquire("missing-artwork", std::filesystem::path("resources/icons/missing.svg"))
            );
        }
    }

    void on_update(float) override {
        if (!m_download.valid() || m_download.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

        try {
            const auto& bytes = m_download.get();
            if (!bytes.empty()) {
                auto image = surface().runtime().textures().acquire("thumbnail:" + m_url, std::string_view(bytes));
                m_loaded = image != nullptr;
                set_texture(std::move(image));
            }
        } catch (const std::exception& error) {
            LOG_ERROR("[artwork] thumbnail failed: {} ({})", error.what(), m_url);
        }

        m_download = {};
        m_spinner->set_visible(false);
        apply_theme_defaults(surface().theme());
    }

private:
    std::string m_url;
    std::shared_future<std::string> m_download;
    SpinnerWidget* m_spinner;
    bool m_loaded = false;
};

bool is_playlist_card(const MusicItem& item) {
    return item.endpoint.video_id.empty() &&
           (item.endpoint.page_type == "MUSIC_PAGE_TYPE_ALBUM" || item.endpoint.page_type == "MUSIC_PAGE_TYPE_PLAYLIST" ||
            item.endpoint.browse_id.starts_with("MPRE") || item.endpoint.browse_id.starts_with("VL"));
}

static std::string subtitle_part(const MusicItem& item, int index = 0) {
    std::string text;
    for (const auto& run : item.subtitle) {
        if (run.text == " • " || run.text == " · ") {
            if (index == 0) break;
            --index;
        } else if (index == 0) {
            text += run.text;
        }
    }

    return text;
}

// prefer linked creators, falling back to the artist segment when the response has no channel link
static std::string card_artist(const MusicItem& item) {
    std::string artist;
    for (const auto& run : item.subtitle) {
        if (!run.endpoint.browse_id.starts_with("UC")) continue;
        if (!artist.empty()) artist += ", ";
        artist += run.text;
    }

    if (!artist.empty()) return artist;

    auto first = subtitle_part(item);
    if (first == "Song" || first == "Video" || first == "Album" || first == "EP" || first == "Single" || first == "Playlist") {
        return subtitle_part(item, 1);
    }

    return first;
}

static std::string playlist_subtitle(const MusicItem& item) {
    const bool album = item.endpoint.page_type == "MUSIC_PAGE_TYPE_ALBUM" || item.endpoint.browse_id.starts_with("MPRE");
    auto type = subtitle_part(item);
    if (type != "EP" && type != "Single") type = album ? "Album" : "Playlist";

    const auto artist = card_artist(item);
    return artist.empty() ? type : type + " by " + artist;
}

static void add_card_text(Container& container, std::string text, bool bold, std::string id) {
    auto& label = container.add<MusiqText>(std::move(text), bold);
    label.set_id(std::move(id));
    label.set_size({grow(), fit()});
    label.set_overflow(TextOverflow::Ellipsis);
}

MediaCardWidget::MediaCardWidget(const MusicItem& item, ThumbnailCache& thumbnails) : Container("", StackDirection::Horizontal) {
    set_type_name("MediaCardWidget");
    set_size({grow(), fit()});
    set_spacing(10.0f);
    set_content_alignment(Anchor::CenterLeft);
    add<CatalogCoverWidget>(item, thumbnails, 48.0f);

    auto& details = add<Container>("details", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(4.0f);
    add_card_text(details, item.title, true, "title");
    add_card_text(details, card_artist(item), false, "artist");
}

void MediaCardWidget::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    configure_all_styles([&theme](Style& style) {
        style.padding({8.0f}).background_color(theme.background_secondary_color).border_radius(8.0f);
    });
}

PlaylistCardWidget::PlaylistCardWidget(const MusicItem& item, ThumbnailCache& thumbnails)
    : Container("", StackDirection::Vertical) {
    set_type_name("PlaylistCardWidget");
    set_size({px(176), fit()});
    set_spacing(8.0f);
    add<CatalogCoverWidget>(item, thumbnails, 176.0f);

    auto& details = add<Container>("details", StackDirection::Vertical);
    details.set_size({grow(), fit()});
    details.set_spacing(4.0f);
    add_card_text(details, item.title, true, "title");
    add_card_text(details, playlist_subtitle(item), false, "metadata");
}
