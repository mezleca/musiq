#include "../session.hpp"
#include "../../utils/log.hpp"

#include <utility>
#include <vector>
#include <algorithm>
#include <cmath>

static const sdbus::InterfaceName player_interface{"org.mpris.MediaPlayer2.Player"};

// https://specifications.freedesktop.org/mpris-spec/latest/Media_Player.html
// https://specifications.freedesktop.org/mpris-spec/latest/Player_Interface.html
MediaSession::MediaSession(const std::string& identifier) {
    try {
        m_connection = sdbus::createSessionBusConnection(sdbus::ServiceName{"org.mpris.MediaPlayer2." + identifier});

        m_object = sdbus::createObject(*m_connection, sdbus::ObjectPath{"/org/mpris/MediaPlayer2"});
        m_object
            ->addVTable(
                // Brings the media player's user interface to the front using any appropriate mechanism available.
                sdbus::registerMethod("Raise").implementedAs([] {}),

                // Causes the media player to stop running.
                // The media player may refuse to allow clients to shut it down. In this case, the CanQuit property is false and
                // this method does nothing.
                sdbus::registerMethod("Quit").implementedAs([] {}),

                // If false, calling Quit will have no effect, and may raise a NotSupported error.
                // If true, calling Quit will cause the media application to attempt to quit (although it may still be prevented
                // from quitting by the user, for example).
                sdbus::registerProperty("CanQuit").withGetter([] { return false; }),

                // If false, calling Raise will have no effect, and may raise a NotSupported error.
                // If true, calling Raise will cause the media application to attempt to bring its user interface to the front,
                // although it may be prevented from doing so (by the window manager, for example).
                sdbus::registerProperty("CanRaise").withGetter([] { return false; }),

                // Indicates whether the /org/mpris/MediaPlayer2 object implements the org.mpris.MediaPlayer2.TrackList interface.
                sdbus::registerProperty("HasTrackList").withGetter([] { return false; }),

                // A friendly name to identify the media player to users.
                // This should usually match the name found in .desktop files
                // (eg: "VLC media player").
                sdbus::registerProperty("Identity").withGetter([] { return std::string{"musiq"}; }),

                // The basename of an installed .desktop file which complies with the Desktop entry specification, with the
                // ".desktop" extension stripped. Example: The desktop entry file is "/usr/share/applications/vlc.desktop", and
                // this property contains "vlc"
                sdbus::registerProperty("DesktopEntry").withGetter([] { return std::string{"org.mezleca.musiq"}; }),

                // The URI schemes supported by the media player.
                // Note that URI schemes should be lower-case.
                sdbus::registerProperty("SupportedUriSchemes").withGetter([] { return std::vector<std::string>{}; }),

                // The mime-types supported by the media player.
                // Mime-types should be in the standard format (eg: audio/mpeg or application/ogg).
                sdbus::registerProperty("SupportedMimeTypes").withGetter([] { return std::vector<std::string>{}; })
            )
            .forInterface(sdbus::InterfaceName{"org.mpris.MediaPlayer2"});

        m_object
            ->addVTable(
                // Starts or resumes playback.
                // If already playing, this has no effect.
                // If paused, playback resumes from the current position.
                // If there is no track to play, this has no effect.
                // If CanPlay is false, attempting to call this method should have no effect.
                sdbus::registerMethod("Play").implementedAs([this] {
                    if (m_playable && m_play_callback) {
                        m_play_callback();
                    }
                }),

                // Pauses playback.
                // If playback is already paused, this has no effect.
                // Calling Play after this should cause playback to start again from the same position.
                // If CanPause is false, attempting to call this method should have no effect.
                sdbus::registerMethod("Pause").implementedAs([this] {
                    if (m_playable && m_pause_callback) {
                        m_pause_callback();
                    }
                }),

                // Pauses playback.
                // If playback is already paused, resumes playback.
                // If playback is stopped, starts playback.
                // If CanPause is false, attempting to call this method should have no effect and raise an error.
                sdbus::registerMethod("PlayPause").implementedAs([this] {
                    const auto& callback = m_status == "Playing" ? m_pause_callback : m_play_callback;
                    if (m_playable && callback) {
                        callback();
                    }
                }),

                // Skips to the next track in the tracklist.
                // If there is no next track (and endless playback and track repeat are both off), stop playback.
                // If playback is paused or stopped, it remains that way.
                // If CanGoNext is false, attempting to call this method should have no effect.
                sdbus::registerMethod("Next").implementedAs([this] {
                    if (m_next && m_next_callback) m_next_callback();
                }),

                // Skips to the previous track in the tracklist.
                // If there is no previous track (and endless playback and track repeat are both off), stop playback.
                // If playback is paused or stopped, it remains that way.
                // If CanGoPrevious is false, attempting to call this method should have no effect.
                sdbus::registerMethod("Previous").implementedAs([this] {
                    if (m_previous && m_previous_callback) m_previous_callback();
                }),

                // Seeks forward in the current track by the specified number of microseconds.
                // A negative value seeks back. If this would mean seeking back further than the start of the track, the position
                // is set to 0. If the value passed in would mean seeking beyond the end of the track, acts like a call to Next.
                // If the CanSeek property is false, this has no effect.
                sdbus::registerMethod("Seek").implementedAs([this](std::int64_t offset) {
                    if (!m_seekable || !m_seek_callback) return;

                    const double seconds = static_cast<double>(m_position) / 1000000 + static_cast<double>(offset) / 1000000;
                    if (seconds > m_duration) {
                        if (m_next && m_next_callback) m_next_callback();
                    } else {
                        m_seek_callback(std::max(0.0, seconds));
                    }
                }),

                // The currently playing track's identifier.
                // If this does not match the id of the currently-playing track, the call is ignored as "stale".
                // /org/mpris/MediaPlayer2/TrackList/NoTrack is not a valid value for this argument.
                // Track position in microseconds.
                // This must be between 0 and <track_length>.
                // Sets the current track position in microseconds.
                // If the Position argument is less than 0, do nothing.
                // If the Position argument is greater than the track length, do nothing.
                // If the CanSeek property is false, this has no effect.
                sdbus::registerMethod("SetPosition").implementedAs([this](const sdbus::ObjectPath& track, std::int64_t position) {
                    if (!m_seekable || !m_seek_callback || position < 0) return;

                    const auto current = m_metadata.find("mpris:trackid");
                    const double seconds = static_cast<double>(position) / 1000000;
                    if (current == m_metadata.end() || current->second.get<sdbus::ObjectPath>() != track ||
                        seconds > m_duration) {
                        return;
                    }
                    m_seek_callback(seconds);
                }),

                // Indicates that the track position has changed in a way that is inconsistant with the current playing state.
                // When this signal is not received, clients should assume that:
                // When playing, the position progresses according to the rate property.
                // When paused, it remains constant.
                // This signal does not need to be emitted when playback starts or when the track changes, unless the track is
                // starting at an unexpected position. An expected position would be the last known one when going from Paused to
                // Playing, and 0 when going from Stopped to Playing.
                sdbus::registerSignal("Seeked").withParameters<std::int64_t>("Position"),

                // When this property changes, the org.freedesktop.DBus.Properties.PropertiesChanged signal is emitted with the
                // new value. The current playback status. May be "Playing", "Paused" or "Stopped".
                sdbus::registerProperty("PlaybackStatus").withGetter([this] { return m_status; }),

                // When this property changes, the org.freedesktop.DBus.Properties.PropertiesChanged signal is emitted with the
                // new value. The current loop / repeat status May be: "None" if the playback will stop when there are no more
                // tracks to play "Track" if the current track will start again from the begining once it has finished playing
                // "Playlist" if the playback loops through a list of tracks
                // If CanControl is false, attempting to set this property should have no effect and raise an error.
                sdbus::registerProperty("LoopStatus")
                    .withGetter([this] { return m_loop; })
                    .withSetter([this](const std::string& status) {
                        if (status != "None" && status != "Track" && status != "Playlist") {
                            throw sdbus::Error(
                                sdbus::Error::Name{"org.freedesktop.DBus.Error.InvalidArgs"}, "Invalid loop status"
                            );
                        }
                        if (m_loop_callback) m_loop_callback(status);
                    }),

                // When this property changes, the org.freedesktop.DBus.Properties.PropertiesChanged signal is emitted with the
                // new value. A value of false indicates that playback is progressing linearly through a playlist, while true
                // means playback is progressing through a playlist in some other order. If CanControl is false, attempting to set
                // this property should have no effect and raise an error.
                sdbus::registerProperty("Shuffle").withGetter([this] { return m_shuffle; }).withSetter([this](bool enabled) {
                    if (m_shuffle_callback) m_shuffle_callback(enabled);
                }),

                // When this property changes, the org.freedesktop.DBus.Properties.PropertiesChanged signal is emitted with the
                // new value. The volume level. When setting, if a negative value is passed, the volume should be set to 0.0. If
                // CanControl is false, attempting to set this property should have no effect and raise an error.
                sdbus::registerProperty("Volume").withGetter([this] { return m_volume; }).withSetter([this](double volume) {
                    if (!std::isfinite(volume)) {
                        throw sdbus::Error(sdbus::Error::Name{"org.freedesktop.DBus.Error.InvalidArgs"}, "Invalid volume");
                    }
                    if (m_volume_callback) m_volume_callback(std::max(0.0, volume));
                }),

                // When this property changes, the org.freedesktop.DBus.Properties.PropertiesChanged signal is emitted with the
                // new value. The metadata of the current element. If there is a current track, this must have a "mpris:trackid"
                // entry (of D-Bus type "o") at the very least, which contains a D-Bus path that uniquely identifies this track.
                sdbus::registerProperty("Metadata").withGetter([this] { return m_metadata; }),

                // The org.freedesktop.DBus.Properties.PropertiesChanged signal is not emitted when this property changes.
                // The current track position in microseconds, between 0 and the 'mpris:length' metadata entry (see Metadata).
                sdbus::registerProperty("Position")
                    .withGetter([this] { return m_position; })
                    .withUpdateBehavior(sdbus::Flags::EMITS_NO_SIGNAL),

                // The current playback rate.
                // If the media player has no ability to play at speeds other than the normal playback rate, this must still be
                // implemented, and must return 1.0. The MinimumRate and MaximumRate properties must also be set to 1.0.
                sdbus::registerProperty("Rate").withGetter([] { return 1.0; }).withSetter([](double) {}),

                // The minimum value which the Rate property can take. Clients should not attempt to set the Rate property below
                // this value. This value should always be 1.0 or less.
                sdbus::registerProperty("MinimumRate").withGetter([] { return 1.0; }),

                // The maximum value which the Rate property can take. Clients should not attempt to set the Rate property above
                // this value. This value should always be 1.0 or greater.
                sdbus::registerProperty("MaximumRate").withGetter([] { return 1.0; }),

                // The org.freedesktop.DBus.Properties.PropertiesChanged signal is not emitted when this property changes.
                // Whether the media player may be controlled over this interface.
                // This property is not expected to change, as it describes an intrinsic capability of the implementation.
                sdbus::registerProperty("CanControl").withGetter([] { return true; }),

                // Whether playback can be started using Play or PlayPause.
                sdbus::registerProperty("CanPlay").withGetter([this] { return m_playable; }),

                // Whether playback can be paused using Pause or PlayPause.
                sdbus::registerProperty("CanPause").withGetter([this] { return m_playable; }),

                // Whether the client can control the playback position using Seek and SetPosition. This may be different for
                // different tracks. If CanControl is false, this property should also be false.
                sdbus::registerProperty("CanSeek").withGetter([this] { return m_seekable; }),

                // Whether the client can call the Next method on this interface and expect the current track to change.
                sdbus::registerProperty("CanGoNext").withGetter([this] { return m_next; }),

                // Whether the client can call the Previous method on this interface and expect the current track to change.
                sdbus::registerProperty("CanGoPrevious").withGetter([this] { return m_previous; })
            )
            .forInterface(player_interface);
    } catch (const sdbus::Error& error) {
        LOG_WARN("[media] session unavailable: {}", error.what());

        m_object.reset();
        m_connection.reset();
    }
}

void MediaSession::update() {
    if (!m_connection) {
        return;
    }

    // dispatch on the playback thread so callbacks never race the queue or player
    try {
        while (m_connection->processPendingEvent()) {
        }
    } catch (const sdbus::Error& error) {
        LOG_WARN("[media] session disconnected: {}", error.what());

        m_object.reset();
        m_connection.reset();
    }
}

void MediaSession::set_metadata(const MediaMetadata& metadata) {
    if (!m_object) {
        return;
    }

    auto& values = m_metadata;
    m_duration = metadata.duration;
    values.clear();

    if (metadata.id != 0) {
        values = {
            {"mpris:trackid", sdbus::Variant{sdbus::ObjectPath{"/com/mezleca/musiq/track/" + std::to_string(metadata.id)}}},
            {"mpris:length", sdbus::Variant{static_cast<std::int64_t>(metadata.duration * 1000000)}},
            {"xesam:title", sdbus::Variant{metadata.title}},
            {"xesam:artist", sdbus::Variant{std::vector<std::string>{metadata.artist}}},
            {"xesam:album", sdbus::Variant{metadata.album}},
            {"mpris:artUrl", sdbus::Variant{metadata.artwork}}
        };
    }

    // When this property changes, the org.freedesktop.DBus.Properties.PropertiesChanged signal is emitted with the new value.
    m_object->emitPropertiesChangedSignal(player_interface, {sdbus::PropertyName{"Metadata"}});
}

void MediaSession::set_playing(bool playing) {
    if (!m_object) return;

    const std::string status = m_metadata.empty() ? "Stopped" : playing ? "Playing" : "Paused";
    if (m_status == status) return;

    m_status = status;

    // When this property changes, the org.freedesktop.DBus.Properties.PropertiesChanged signal is emitted with the new value.
    m_object->emitPropertiesChangedSignal(player_interface, {sdbus::PropertyName{"PlaybackStatus"}});
}

void MediaSession::set_position(double seconds) {
    m_position = static_cast<std::int64_t>(seconds * 1000000);
}

void MediaSession::seeked(double seconds) {
    set_position(seconds);

    if (m_object) {
        m_object->emitSignal("Seeked").onInterface(player_interface).withArguments(m_position);
    }
}

void MediaSession::set_controls(bool playable, bool next, bool previous, bool seekable) {
    if (!m_object) return;
    if (m_playable == playable && m_next == next && m_previous == previous && m_seekable == seekable) return;

    m_playable = playable;
    m_next = next;
    m_previous = previous;
    m_seekable = seekable;

    // When this property changes, the org.freedesktop.DBus.Properties.PropertiesChanged signal is emitted with the new value.
    m_object->emitPropertiesChangedSignal(
        player_interface, {sdbus::PropertyName{"CanPlay"}, sdbus::PropertyName{"CanPause"}, sdbus::PropertyName{"CanGoNext"},
                           sdbus::PropertyName{"CanGoPrevious"}, sdbus::PropertyName{"CanSeek"}}
    );
}

void MediaSession::on_play(std::function<void()> callback) {
    m_play_callback = std::move(callback);
}

void MediaSession::on_pause(std::function<void()> callback) {
    m_pause_callback = std::move(callback);
}

void MediaSession::on_next(std::function<void()> callback) {
    m_next_callback = std::move(callback);
}

void MediaSession::on_previous(std::function<void()> callback) {
    m_previous_callback = std::move(callback);
}

void MediaSession::on_seek(std::function<void(double)> callback) {
    m_seek_callback = std::move(callback);
}

void MediaSession::set_shuffle(bool enabled) {
    if (!m_object || m_shuffle == enabled) return;

    m_shuffle = enabled;
    m_object->emitPropertiesChangedSignal(player_interface, {sdbus::PropertyName{"Shuffle"}});
}

void MediaSession::set_loop(const std::string& status) {
    if (!m_object || m_loop == status) return;

    m_loop = status;
    m_object->emitPropertiesChangedSignal(player_interface, {sdbus::PropertyName{"LoopStatus"}});
}

void MediaSession::set_volume(double volume) {
    if (!m_object || m_volume == volume) return;

    m_volume = volume;
    m_object->emitPropertiesChangedSignal(player_interface, {sdbus::PropertyName{"Volume"}});
}

void MediaSession::on_shuffle(std::function<void(bool)> callback) {
    m_shuffle_callback = std::move(callback);
}

void MediaSession::on_loop(std::function<void(const std::string&)> callback) {
    m_loop_callback = std::move(callback);
}

void MediaSession::on_volume(std::function<void(double)> callback) {
    m_volume_callback = std::move(callback);
}
