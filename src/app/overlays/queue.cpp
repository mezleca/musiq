#include "queue.hpp"
#include "../widgets/catalog-card.hpp"
#include "../widgets/text.hpp"
#include "../../playback/controller.hpp"

#include <imgui-ui/widgets/checkbox.hpp>
#include <imgui-ui/layout/virtual-layout.hpp>
#include <imgui-ui/surface.hpp>
#include <format>
#include <utility>
#include <unordered_set>

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

    auto& close = heading.add<MusiqIconButtonWidget>("icons/close.svg");
    close.set_tooltip("Close queue");
    close.on_click([this] { set_visible(false); });

    m_status = &panel.add<MusiqText>("");

    m_tracks = &panel.add<VirtualLayout>("tracks", 64.0f);
    m_tracks->set_size({grow(), grow()});
    m_tracks->set_spacing(6.0f);
    m_tracks->set_overscan(2);
    m_list.emplace(*m_tracks);
    m_tracks->set_items(0, [this](std::size_t index) -> Node& {
        const auto& entry = m_playback.queue().entries()[index];
        if (auto* card = m_list->find(entry.id)) return *card;

        auto& card = m_list->add(entry.id, entry.media, m_thumbnails);
        card.on_click([this, id = entry.id] { m_playback.queue().select(id); });

        auto& remove = card.add<MusiqIconButtonWidget>("icons/close.svg");
        remove.set_tooltip("Remove from queue");
        remove.on_event(EventType::Click, [this, id = entry.id](UiEvent& event) {
            m_playback.remove(id);
            event.stop_propagation();
        });
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

    const auto current_id = current ? std::make_optional(current->id) : std::nullopt;

    // cards stay mapped by entry id, so a removal only drops the matching card
    if (queue.revision() != m_revision) {
        m_revision = queue.revision();
        std::unordered_set<std::uint64_t> ids;
        ids.reserve(entries.size());
        for (const auto& entry : entries) {
            ids.insert(entry.id);
        }
        m_list->refresh([&ids](std::uint64_t id) { return ids.contains(id); }, current_id);

        m_tracks->set_items(entries.size());
    } else {
        m_list->set_current(current_id);
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
