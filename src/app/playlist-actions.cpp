#include "app.hpp"
#include "widgets/spinner.hpp"
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

class PlaylistDestinationWidget final : public Widget {
public:
    PlaylistDestinationWidget(std::string title, TextureHandle check, std::shared_ptr<PlaylistAddStatus> status)
        : Widget({}, StackDirection::Horizontal, "PlaylistDestination", InputMode::None), m_status(std::move(status)) {
        set_size({grow(), grow()});
        m_label = &add<TextWidget>(std::move(title));
        m_label->set_size({grow(), grow()});
        m_label->set_alignment(Anchor::CenterLeft);
        m_label->set_overflow(TextOverflow::Ellipsis);
        m_label->set_input_mode(InputMode::None);

        m_feedback = &add<Widget>("feedback", StackDirection::Horizontal, "PlaylistAddFeedback", InputMode::None);
        m_feedback->set_layout({
            .size = {grow(), grow()},
            .placement = {.anchor = Anchor::Center, .origin = Anchor::Center},
            .in_flow = false,
        });
        m_feedback->set_content_alignment(Anchor::Center);
        m_feedback->set_opacity(0.0f, {0.0f});
        m_spinner = &m_feedback->add<SpinnerWidget>();
        m_check = &m_feedback->add<ImageWidget>(std::move(check));
        m_check->set_input_mode(InputMode::None);
        m_check->set_visible(false);
    }

private:
    void apply_theme_defaults(const Theme& theme) override {
        Widget::apply_theme_defaults(theme);
        const auto size = px(theme.controls.font_size);
        m_spinner->set_size({size, size});
        m_check->set_size({size, size});
    }

    void on_update(float) override {
        const auto& menu = static_cast<const ContextMenuWidget&>(*parent()->parent());
        if (!menu.is_open()) {
            if (m_displayed != PlaylistAddStatus::Idle || *m_status != PlaylistAddStatus::Idle) {
                *m_status = m_displayed = PlaylistAddStatus::Idle;
                animator().cancel();
                parent()->set_enabled(true);
                m_label->set_opacity(1.0f, {0.0f});
                m_feedback->set_opacity(0.0f, {0.0f});
            }
            return;
        }
        if (m_displayed == *m_status) return;
        m_displayed = *m_status;
        const bool working = m_displayed == PlaylistAddStatus::Pending;
        const bool done = m_displayed == PlaylistAddStatus::Succeeded;
        if (working || done) {
            m_spinner->set_visible(working);
            m_check->set_visible(done);
        }
        parent()->set_enabled(!working && !done);
        m_label->set_opacity(working || done ? 0.0f : 1.0f, {0.15f});
        m_feedback->set_opacity(working || done ? 1.0f : 0.0f, {0.15f});
        if (done) {
            m_check->set_opacity(0.0f, {0.0f});
            m_check->fade_in({0.15f});
            animator().animate().delay(1.5f).end([status = m_status] {
                if (*status == PlaylistAddStatus::Succeeded) *status = PlaylistAddStatus::Idle;
            });
        }
    }

    std::shared_ptr<PlaylistAddStatus> m_status;
    TextWidget* m_label = nullptr;
    Widget* m_feedback = nullptr;
    SpinnerWidget* m_spinner = nullptr;
    ImageWidget* m_check = nullptr;
    PlaylistAddStatus m_displayed = PlaylistAddStatus::Idle;
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
    const auto track = std::make_shared<MusicItem>(item);
    for (const auto& playlist : m_catalog.library_playlists()) {
        if (!playlist.playlist_owned || is_liked_music_playlist(playlist.endpoint.browse_id)) continue;

        const auto status = std::make_shared<PlaylistAddStatus>(PlaylistAddStatus::Idle);
        auto destination =
            ContextMenuItem::action(playlist.title, [this, id = playlist.endpoint.browse_id, track, status](ContextMenuWidget&) {
                if (*status == PlaylistAddStatus::Idle) add_playlist_track(id, *track, status);
            });
        destination.close_on_activate = false;
        destination.build_content = [title = playlist.title, check, status](Widget& row) {
            row.add<PlaylistDestinationWidget>(title, check, status);
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

void MusiqApp::add_playlist_track(std::string playlist_id, MusicItem track, std::shared_ptr<PlaylistAddStatus> status) {
    const auto key = playlist_id + '\n' + track.endpoint.video_id;
    if (!m_pending_playlist_additions.insert(key).second) return;

    *status = PlaylistAddStatus::Pending;
    MusicPlaylistAddRequest request{.playlist_id = playlist_id, .video_ids = {track.endpoint.video_id}};

    m_tasks.enqueue(
        [session = m_session.youtube(), request = std::move(request)] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).add(request);
        },
        [this, status, key, playlist_id = std::move(playlist_id), track = std::move(track),
         identity = m_session.youtube().identity()](TaskOutcome<YoutubeResult<MusicPlaylistEditResult>> outcome) mutable {
            m_pending_playlist_additions.erase(key);
            const auto result = youtube_result(std::move(outcome));
            if (!result) {
                *status = PlaylistAddStatus::Idle;
                LOG_WARN("[playlist] add failed for {}: {}", playlist_id, result.error().message);
                return;
            }

            if (*status == PlaylistAddStatus::Pending) *status = PlaylistAddStatus::Succeeded;
            if (identity != m_session.youtube().identity()) return;

            const auto view = m_playlists.find(playlist_id);
            if (view != m_playlists.end()) {
                const auto added = std::ranges::find(result->added, track.endpoint.video_id, &MusicPlaylistAddedEntry::video_id);
                track.playlist_set_video_id = added == result->added.end() ? std::string{} : added->set_video_id;
                if (track.playlist_set_video_id.empty() || !view->second->append_added_track(std::move(track))) {
                    view->second->load(m_session.youtube(), PlaylistLoad::Reload);
                    return;
                }
            }
            refresh_playlist_cover(playlist_id);
        }
    );
}

void MusiqApp::remove_playlist_track(std::string playlist_id, MusicPlaylistRemoveRequest::Entry entry) {
    const auto set_video_id = entry.set_video_id;
    MusicPlaylistRemoveRequest request{.playlist_id = playlist_id, .entries = {std::move(entry)}};
    const auto browse_id = playlist_browse_id(playlist_id);
    m_tasks.enqueue(
        [session = m_session.youtube(), request = std::move(request)] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).remove(request);
        },
        [this, browse_id, set_video_id,
         identity = m_session.youtube().identity()](TaskOutcome<YoutubeResult<MusicPlaylistEditResult>> outcome) {
            if (identity != m_session.youtube().identity()) return;
            const auto view = m_playlists.find(browse_id);
            const auto result = youtube_result(std::move(outcome));
            if (!result) {
                if (view != m_playlists.end()) view->second->show_action_error(result.error().message);
                return;
            }

            if (view != m_playlists.end()) view->second->remove_track(set_video_id);
            refresh_playlist_cover(browse_id);
        }
    );
}

void MusiqApp::refresh_playlist_cover(std::string browse_id) {
    m_tasks.enqueue(
        [session = m_session.youtube(), browse_id] {
            const YoutubeMusicClient client(session);
            return YoutubeMusicPlaylists(client).get(browse_id);
        },
        [this, browse_id, identity = m_session.youtube().identity()](TaskOutcome<YoutubeResult<MusicPlaylist>> outcome) {
            if (identity != m_session.youtube().identity()) return;
            const auto result = youtube_result(std::move(outcome));
            if (result) update_playlist_cover(browse_id, result->thumbnails);
        }
    );
}

void MusiqApp::update_playlist_cover(const std::string& browse_id, const std::vector<MusicThumbnail>& thumbnails) {
    if (const auto view = m_playlists.find(browse_id); view != m_playlists.end()) {
        view->second->update_cover(thumbnails);
    }
    m_catalog.update_playlist_cover(browse_id, thumbnails);
    m_playlist_editor->update_cover(browse_id, thumbnails);
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
