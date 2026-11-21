#include "queue.hpp"
#include "../widgets/button.hpp"
#include "../widgets/catalog-card.hpp"
#include "../widgets/text.hpp"
#include "../../playback/controller.hpp"

#include <arbor/surface.hpp>
#include <algorithm>
#include <format>
#include <utility>
#include <unordered_set>

using namespace ui;

class QueueLoadMoreButton final : public MusiqIconButtonWidget {
public:
    QueueLoadMoreButton() : MusiqIconButtonWidget("icons/plus.svg", 40.0f, 20.0f) {}

protected:
    void apply_theme_defaults(const Theme& theme) override {
        MusiqIconButtonWidget::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) {
            style.background_color(theme.accent_color).color(rgb(255, 255, 255)).border_radius(20.0f);
        });
        style(StyleType::HOVER).background_color(theme.accent_hover_color);
        style(StyleType::ACTIVE).background_color(theme.accent_color);
    }
};

QueueOverlay::QueueOverlay(PlaybackController& playback, ThumbnailCache& thumbnails, MediaMenu menu)
    : ModalOverlay("queue"), m_playback(playback), m_thumbnails(thumbnails), m_menu(std::move(menu)) {
    set_type_name("QueueOverlay");
    auto& panel = content();
    panel.set_size({{px(0), px(680)}, percent(75)});

    auto& heading = panel.add<Container>("heading", StackDirection::Horizontal);
    heading.set_size({grow(), fit()});
    heading.set_content_alignment(Anchor::CenterLeft);
    auto& title = heading.add<MusiqText>("Queue", FontWeight::Bold, 22.0f);
    title.set_size({grow(), fit()});

    auto& close = heading.add<MusiqIconButtonWidget>("icons/close.svg");
    close.on_click([this] { set_visible(false); });

    m_status = &panel.add<MusiqText>("");

    m_tracks = &panel.add<TrackList>();
    m_tracks->set_size({grow(), grow()});
    m_tracks->set_spacing(6.0f);
    m_tracks->set_overscan(2);
    m_tracks->set_items(1, [this](std::size_t index) -> Node& {
        const auto& entries = m_playback.queue().entries();
        if (index == entries.size()) {
            if (!m_more_row) {
                m_more_row = &m_tracks->add<Container>("load-more", StackDirection::Horizontal);
                m_more_row->set_size({grow(), px(64)});
                m_more_row->set_content_alignment(Anchor::Center);
                m_more_row->configure_all_styles([](Style& style) { style.padding({0.0f, 8.0f}); });
                m_more = &m_more_row->add<QueueLoadMoreButton>();
                m_more->set_enabled(m_playback.queue().current() && !m_playback.page_loading());
                m_more->on_click([this] { m_playback.load_more(); });
            }

            return *m_more_row;
        }

        const auto& entry = entries[index];
        if (auto* card = m_tracks->find_track(entry.id)) {
            return *card;
        }

        auto& card = m_tracks->add_track(entry.id, entry.media, m_thumbnails);
        card.on_click([this, id = entry.id] { m_playback.queue().select(id); });

        if (m_menu) {
            attach_context_menu(card, [this, id = entry.id](ImVec2 position) {
                const auto& entries = m_playback.queue().entries();
                const auto found = std::ranges::find(entries, id, &QueueEntry::id);
                if (found == entries.end()) return false;

                return m_menu(found->media, {.playlist_id = {}, .queue_entry_id = id}, position);
            });
        }

        auto& remove = card.add<MusiqIconButtonWidget>("icons/close.svg");
        remove.on_event(EventType::Click, [this, id = entry.id](UiEvent& event) {
            m_playback.remove(id);
            event.stop_propagation();
        });

        return card;
    });
}

void QueueOverlay::show() {
    set_visible(true);
    refresh(true);
}

void QueueOverlay::refresh(bool entries_changed) {
    if (!visible()) return;

    auto& queue = m_playback.queue();
    const auto& entries = queue.entries();
    const auto* current = queue.current();

    m_tracks->set_current(current ? current->media.endpoint.video_id : "");

    // cards stay mapped by entry id, so a removal only drops the matching card
    if (entries_changed) {
        std::unordered_set<std::uint64_t> ids;
        ids.reserve(entries.size());
        for (const auto& entry : entries) {
            ids.insert(entry.id);
        }
        m_tracks->retain([&ids](std::uint64_t id) { return ids.contains(id); });

        m_tracks->set_items(entries.size() + 1);
    }

    std::string status;
    if (!m_playback.error().empty()) {
        status = m_playback.error();
    } else if (m_playback.loading()) {
        status = "Loading tracks...";
    } else if (entries.empty()) {
        status = "The queue is empty.";
    } else {
        status = std::format("{} tracks", entries.size());
    }
    m_status->set_text(std::move(status));

    if (m_more) {
        m_more->set_enabled(current && !m_playback.page_loading());
    }
}
