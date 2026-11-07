#include "queue.hpp"
#include "../widgets/catalog-card.hpp"
#include "../widgets/text.hpp"
#include "../../playback/controller.hpp"

#include <imgui-ui/widgets/checkbox.hpp>
#include <imgui-ui/layout/virtual-layout.hpp>
#include <imgui-ui/surface.hpp>
#include <format>
#include <utility>
#include <algorithm>

using namespace ui;

QueueOverlay::QueueOverlay(PlaybackController& playback, ThumbnailCache& thumbnails)
    : ModalOverlay("queue"), m_playback(playback), m_thumbnails(thumbnails) {
    set_type_name("QueueOverlay");
    auto& panel = content();
    panel.set_size({{px(0), px(680)}, percent(75)});

    auto& heading = panel.add<Container>("heading", StackDirection::Horizontal);
    heading.set_size({grow(), fit()});
    heading.set_content_alignment(Anchor::CenterLeft);
    auto& title = heading.add<MusiqText>("Queue", true);
    title.set_size({grow(), fit()});

    auto& close = heading.add<MusiqIconButtonWidget>("resources/icons/close.svg");
    close.set_tooltip("Close queue");
    close.on_click([this] { set_visible(false); });

    m_status = &panel.add<MusiqText>("");

    m_tracks = &panel.add<VirtualLayout>("tracks", 64.0f);
    m_tracks->set_size({grow(), grow()});
    m_tracks->set_spacing(6.0f);
    m_tracks->set_overscan(2);
    m_tracks->set_items(0, [this](std::size_t index) -> Node& {
        const auto& entry = m_playback.queue().entries()[index];
        auto [row, created] = m_rows.try_emplace(entry.id, nullptr);
        if (!created) return *row->second;

        auto& card = m_tracks->add<MediaCardWidget>(entry.media, m_thumbnails);
        card.set_id(std::to_string(entry.id));
        card.on_click([this, id = entry.id] { m_playback.queue().select(id); });
        card.set_selected(index == m_playback.queue().position());

        auto& remove = card.add<MusiqIconButtonWidget>("resources/icons/close.svg");
        remove.set_tooltip("Remove from queue");
        remove.on_event(EventType::Click, [this, id = entry.id](UiEvent& event) {
            m_playback.remove(id);
            event.stop_propagation();
        });

        row->second = &card;
        return card;
    });

    auto& footer = panel.add<Container>("footer", StackDirection::Horizontal);
    footer.set_size({grow(), fit()});
    footer.set_content_alignment(Anchor::CenterLeft);

    auto& autoplay = footer.add<CheckboxWidget>(m_autoplay, "Autoplay");
    autoplay.set_size({grow(), fit()});
    autoplay.on_change([this] { m_playback.set_autoplay(m_autoplay); });

    m_more = &footer.add<MusiqButtonWidget>("Load more");
    m_more->on_click([this] { m_playback.load_more(); });
}

void QueueOverlay::show() {
    set_visible(true);
    refresh();
}

void QueueOverlay::refresh() {
    if (!visible()) return;

    m_autoplay = m_playback.autoplay();
    auto& queue = m_playback.queue();
    const auto& entries = queue.entries();
    const auto* current = queue.current();

    // rows stay mapped by entry id, so a removal only drops the matching card
    if (queue.revision() != m_revision) {
        m_revision = queue.revision();
        for (auto row = m_rows.begin(); row != m_rows.end();) {
            const bool present = std::ranges::find(entries, row->first, &QueueEntry::id) != entries.end();
            if (present) {
                ++row;
                continue;
            }

            m_tracks->remove(*row->second);
            row = m_rows.erase(row);
        }

        m_tracks->set_items(entries.size());
        for (const auto& [id, row] : m_rows) {
            row->set_selected(current && current->id == id);
        }
    }

    std::string status;
    if (!m_playback.error().empty()) {
        status = m_playback.error();
    } else if (m_playback.loading()) {
        status = "Loading tracks...";
    } else if (queue.entries().empty()) {
        status = "The queue is empty.";
    } else {
        status = std::format("{} tracks", queue.entries().size());
    }
    m_status->set_text(std::move(status));

    m_more->set_visible(m_playback.has_more());
    m_more->set_enabled(!m_playback.loading());
}

void QueueOverlay::event(UiEvent& event) {
    const bool escape = event.type == EventType::Cancel || (event.type == EventType::KeyDown && event.key == Key::Escape);
    const bool outside = event.type == EventType::PointerDown && event.button == PointerButton::Left &&
                         !content().layout().visual_rect().contains(event.position);
    if (escape || outside) {
        set_visible(false);
        event.block_native_input();
        event.stop_propagation();
    }
}
