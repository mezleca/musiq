#include "app.hpp"
#include "../utils/media.hpp"
#include "../utils/log.hpp"
#include "../utils/path.hpp"
#include "../youtube/client.hpp"
#include "../youtube/playlists.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <arbor/resources/texture-registry.hpp>
#include <arbor/widgets/image.hpp>
#include <arbor/widgets/text.hpp>

using namespace ui;
using namespace utils::media;

enum class PlaylistAddStatus {
    Idle,
    Pending,
    Succeeded
};

struct PlaylistAddState {
    PlaylistAddStatus status = PlaylistAddStatus::Idle;
};

class PlaylistDestinationWidget final : public Widget {
public:
    PlaylistDestinationWidget(std::string title, TextureHandle check, std::shared_ptr<PlaylistAddState> state)
        : Widget({}, StackDirection::Horizontal, "PlaylistDestination", InputMode::None), m_state(std::move(state)) {
        set_size({grow(), grow()});
        set_spacing(6.0f);
        set_content_alignment(Anchor::CenterLeft);
        auto& label = add<TextWidget>(std::move(title));
        label.set_size({grow(), grow()});
        label.set_alignment(Anchor::CenterLeft);
        label.set_overflow(TextOverflow::Ellipsis);
        m_check = &add<ImageWidget>(std::move(check));
        m_check->set_size({px(16), px(16)});
        m_check->set_input_mode(InputMode::None);
        m_check->set_opacity(0.0f, {0.0f});
    }

private:
    void on_update(float) override {
        if (m_state->status == PlaylistAddStatus::Succeeded && !m_confirmed) {
            m_confirmed = true;
            m_check->set_opacity(1.0f, {0.0f});
            m_check->animator().animate().delay(2.0f).end([check = m_check] { check->fade_out({0.25f}); });
        }
    }

    std::shared_ptr<PlaylistAddState> m_state;
    ImageWidget* m_check = nullptr;
    bool m_confirmed = false;
};

struct PlaylistCreation {
    std::string id;
    bool has_cover = false;
};

void MusiqApp::save_playlist(PlaylistDraft draft) {
    if (draft.mode == PlaylistEditorMode::Save) {
        create_playlist(std::move(draft));
    } else {
        edit_playlist(std::move(draft));
    }
}

void MusiqApp::create_playlist(PlaylistDraft&& draft) {
    if (!m_session.youtube().user()) {
        m_playlist_editor->fail("Sign in to create playlists.");
        return;
    }

    MusicPlaylistCreateRequest request;
    request.title = draft.title;
    request.description = std::move(draft.description);
    request.privacy = draft.privacy;

    m_tasks.enqueue(
        [session = m_session.youtube(), request = std::move(request),
         cover_path = std::move(draft.cover_path)]() -> YoutubeResult<PlaylistCreation> {
            const YoutubeMusicClient client(session);
            const YoutubeMusicPlaylists playlists(client);
            auto created = playlists.create(request);
            if (!created) return std::unexpected(std::move(created.error()));

            PlaylistCreation result{.id = std::move(created->id)};

            // attach the cover before opening the page so creation does not trigger two loads
            if (!cover_path.empty()) {
                MusicPlaylistUpdateRequest cover_request;
                cover_request.playlist_id = result.id;
                cover_request.thumbnail_path = cover_path;
                const auto cover = playlists.update(cover_request);
                if (!cover) {
                    LOG_WARN("[playlist] cover upload failed for {}: {}", result.id, cover.error().message);
                }
                result.has_cover = cover.has_value();
            }
            return result;
        },
        [this, title = std::move(draft.title),
         identity = m_session.youtube().identity()](TaskOutcome<YoutubeResult<PlaylistCreation>> outcome) mutable {
            if (identity != m_session.youtube().identity()) return;
            auto result = youtube_result(std::move(outcome));
            if (!result) {
                m_playlist_editor->fail(result.error().message);
                return;
            }

            MusicItem item;
            item.title = std::move(title);
            item.endpoint.browse_id = playlist_browse_id(result->id);
            item.playlist_owned = true;
            m_catalog.add_created_playlist(item);
            m_playlist_editor->finish();

            const auto load = result->has_cover ? PlaylistLoad::NewlyCreatedWithCover : PlaylistLoad::NewlyCreated;
            open_playlist(std::move(item), load);
        }
    );
}

void MusiqApp::edit_playlist(PlaylistDraft&& draft) {
    MusicPlaylistUpdateRequest request;
    request.playlist_id = draft.id;
    request.title = std::move(draft.title);
    request.description = std::move(draft.description);
    request.privacy = draft.privacy;

    if (!draft.cover_path.empty()) {
        request.thumbnail_path = std::move(draft.cover_path);
    }

    m_tasks.enqueue(
        [session = m_session.youtube(), request = std::move(request)] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).update(request);
        },
        [this, id = std::move(draft.id)](TaskOutcome<YoutubeResult<MusicPlaylistEditResult>> outcome) {
            auto result = youtube_result(std::move(outcome));
            if (!result) {
                m_playlist_editor->fail(result.error().message);
                return;
            }

            m_catalog.refresh_library();
            m_playlist_editor->finish();

            refresh_playlist(id);
        }
    );
}

void MusiqApp::delete_playlist(std::string id) {
    const auto& session = m_session.youtube();
    if (!session.user()) {
        m_playlist_editor->fail("Sign in to delete playlists.");
        return;
    }

    const NavigationRoute route{"playlist", playlist_browse_id(id)};
    m_tasks.enqueue(
        [session, id = std::move(id)] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).erase(id);
        },
        [this, route](TaskOutcome<YoutubeResult<void>> outcome) {
            const auto result = youtube_result(std::move(outcome));
            if (!result) {
                m_playlist_editor->fail(result.error().message);
                return;
            }

            m_playlist_editor->finish();
            m_catalog.remove_playlist(route.query);

            m_history.discard(route);
            std::erase(m_page_order, route);
            show_page();

            const auto view = m_playlists.find(route.query);
            if (view != m_playlists.end()) {
                m_pages->remove(*view->second);
                m_playlists.erase(view);
            }

            m_playlist_items.erase(route.query);
        }
    );
}

void MusiqApp::unsave_playlist(std::string id) {
    const auto browse_id = playlist_browse_id(id);
    m_tasks.enqueue(
        [session = m_session.youtube(), id = std::move(id)] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).unsave(id);
        },
        [this, browse_id, identity = m_session.youtube().identity()](TaskOutcome<YoutubeResult<void>> outcome) {
            if (identity != m_session.youtube().identity()) return;
            const auto result = youtube_result(std::move(outcome));
            if (!result) {
                LOG_WARN("[playlist] library removal failed for {}: {}", browse_id, result.error().message);
                const auto view = m_playlists.find(browse_id);
                if (view != m_playlists.end()) view->second->show_action_error(result.error().message);
                return;
            }
            m_catalog.remove_playlist(browse_id);
        }
    );
}

MediaMenu MusiqApp::media_menu_handler() {
    return [this](const MusicItem& item, MediaMenuContext context, ImVec2 position) {
        auto items = media_menu(item, context);
        if (items.empty()) {
            return false;
        }

        m_context_menu->set_items(std::move(items));
        m_context_menu->open_at(position, context.active_track);
        return true;
    };
}

ContextMenuItems MusiqApp::media_menu(const MusicItem& item, MediaMenuContext context) {
    ContextMenuItems items;
    const bool signed_in = m_session.youtube().user().has_value();

    if (is_playlist_card(item)) {
        add_menu_action(items, is_album(item.endpoint) ? "Open album" : "Open playlist", [this, item] { open_playlist(item); });
        if (!signed_in || is_liked_music_playlist(item.endpoint.browse_id)) return items;

        const auto view = m_playlists.find(item.endpoint.browse_id);
        if (view != m_playlists.end() && view->second->editable()) {
            add_menu_action(items, "Edit playlist", [this, id = item.endpoint.browse_id] {
                const auto current = m_playlists.find(id);
                if (current != m_playlists.end() && current->second->editable()) {
                    m_playlist_editor->show_edit(current->second->playlist());
                }
            });
        }

        if (context.library_entry && !is_album(item.endpoint)) {
            if (item.playlist_owned) {
                add_menu_action(items, "Delete playlist", [this, id = item.endpoint.browse_id, title = item.title] {
                    m_playlist_editor->show_delete(id, title);
                });
            } else {
                add_menu_action(items, "Remove from library", [this, id = item.endpoint.browse_id] { unsave_playlist(id); });
            }
        }

        return items;
    }

    if (!is_playable(item)) {
        return items;
    }

    const auto& video_id = item.endpoint.video_id;
    const bool queued =
        context.queue_entry_id != 0 || std::ranges::any_of(m_playback.queue().entries(), [&video_id](const QueueEntry& entry) {
            return entry.media.endpoint.video_id == video_id;
        });
    if (!queued) {
        const auto track = std::make_shared<MusicItem>(item);
        add_menu_action(items, "Play next", [this, track] { m_playback.queue().play_next(*track); });
        add_menu_action(items, "Add to queue", [this, track] { m_playback.queue().append(*track); });
    } else if (context.queue_entry_id != 0) {
        add_menu_action(items, "Remove from queue", [this, id = context.queue_entry_id] { m_playback.remove(id); });
    }

    if (context.active_track) {
        const auto* current = m_playback.queue().current();
        if (current && !current->playlist_source.empty()) {
            add_menu_action(items, "Show on playlist", [this, source = current->playlist_source, track = item] {
                show_on_playlist(source, track);
            });
        }
    }

    if (!signed_in) {
        return items;
    }

    ContextMenuItems destinations;
    const auto check = m_runtime.textures().acquire("icons/check.svg", utils::path::from_resources("icons/check.svg"));
    for (const auto& playlist : m_catalog.library_playlists()) {
        if (!playlist.playlist_owned || is_liked_music_playlist(playlist.endpoint.browse_id)) continue;

        auto state = std::make_shared<PlaylistAddState>();
        auto destination = ContextMenuItem::action(
            playlist.title, [this, id = playlist.endpoint.browse_id, video_id, state](ContextMenuWidget&) {
                add_playlist_track(id, video_id, state);
            }
        );
        destination.close_on_activate = false;
        destination.build_content = [title = playlist.title, check, state](Widget& row) {
            row.add<PlaylistDestinationWidget>(title, check, state);
        };
        destinations.push_back(std::move(destination));
    }
    if (!destinations.empty()) {
        items.push_back(ContextMenuItem::submenu("Add to playlist", std::move(destinations)));
    }

    if (!context.playlist_id.empty() && !item.playlist_set_video_id.empty()) {
        MusicPlaylistRemoveRequest::Entry entry{.video_id = video_id, .set_video_id = item.playlist_set_video_id};
        add_menu_action(items, "Remove from playlist", [this, id = std::string(context.playlist_id), entry = std::move(entry)] {
            remove_playlist_track(id, entry);
        });
    }

    return items;
}

void MusiqApp::add_playlist_track(std::string playlist_id, std::string video_id, std::shared_ptr<PlaylistAddState> state) {
    if (state->status != PlaylistAddStatus::Idle) return;
    std::erase_if(m_playlist_additions, [](const auto& addition) { return addition.second.expired(); });
    const auto key = playlist_id + '\n' + video_id;
    const auto previous = m_playlist_additions.find(key);
    if (previous != m_playlist_additions.end()) {
        const auto active = previous->second.lock();
        if (active && active->status != PlaylistAddStatus::Idle) return;
    }
    m_playlist_additions[key] = state;
    state->status = PlaylistAddStatus::Pending;
    MusicPlaylistAddRequest request;
    request.playlist_id = playlist_id;
    request.video_ids = {video_id};

    m_tasks.enqueue(
        [session = m_session.youtube(), request = std::move(request)] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).add(request);
        },
        [this, state, playlist_id, key](TaskOutcome<YoutubeResult<MusicPlaylistEditResult>> outcome) {
            const auto result = youtube_result(std::move(outcome));
            if (result) {
                state->status = PlaylistAddStatus::Succeeded;
                refresh_playlist(playlist_id);
            } else {
                state->status = PlaylistAddStatus::Idle;
                LOG_WARN("[playlist] add failed for {}: {}", playlist_id, result.error().message);
                m_playlist_additions.erase(key);
            }
        }
    );
}

void MusiqApp::remove_playlist_track(std::string playlist_id, MusicPlaylistRemoveRequest::Entry entry) {
    MusicPlaylistRemoveRequest request{.playlist_id = playlist_id, .entries = {std::move(entry)}};
    const auto browse_id = playlist_browse_id(playlist_id);
    m_tasks.enqueue(
        [session = m_session.youtube(), request = std::move(request)] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).remove(request);
        },
        [this, browse_id](TaskOutcome<YoutubeResult<MusicPlaylistEditResult>> outcome) {
            const auto result = youtube_result(std::move(outcome));
            if (!result) {
                const auto view = m_playlists.find(browse_id);
                if (view != m_playlists.end()) view->second->show_action_error(result.error().message);
                return;
            }

            refresh_playlist(browse_id);
        }
    );
}

void MusiqApp::refresh_playlist(const std::string& playlist_id) {
    const auto browse_id = playlist_browse_id(playlist_id);
    const auto view = m_playlists.find(browse_id);
    if (view == m_playlists.end()) {
        return;
    }

    if (view->second == m_current_page) {
        view->second->load(m_session.youtube(), PlaylistLoad::Reload);
    } else {
        view->second->invalidate();
    }
}
