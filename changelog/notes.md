# Unreleased
- Sidebar: update the playlist cover properly on new tracks
- ContextMenu: improve "add to playlist" item feedback
- Playlist: filter out liked music and not owned playlist from "add to playlist" menu and other actions
- PlaylistOverlay: fixed unecessary reload while removing a track

# v0.7.1
- Sidebar: fixed newly created playlist not appearing
- PlaylistRemoval: use a simple confirmation instead of re-using the edit one

# v0.7.0
- Queue: rework queue creation on playlist (first track should load a bit faster and queue will hold all of the songs instead of limiting itself to 100)
- QueueModal: updated "load more" button
- Playlist: allow removing and editing cover from created playlist
- PlaylistOverlay: added placeholder icon for creation modal's cover
- PlaylistOverlay: added recommended songs section
- PlaylistOverlay: copy playlist cover to the edit modal
- PlaylistOverlay: fixed newly created playlist failing to load properly
- PlaylistOverlay: fixed stretched playlist cover
- SearchBox: fixed input losing focus while active
- SearchBox: only show search suggestions if input is focused
- SearchOverlay: remove unecessary spacing between tracks
- SearchOverlay: organize playlists using wrap containers
- SettingsOverlay: update interface scale on mouse down instead of debounce
- UI: added menu triggers (context menu) to all available widgets

# v0.6.1
- Tests: Use a local python http server to test endpoint parsing, etc 
- Core(ThreadPool): Separate workers by priority
- Core: Simplified a bunch of fetch tasks by removing uncessary logic and using basic state machines
- UI: Updated framework to the latest commit
- UI: Removed a bunch of unecessary tooltips
- UI(Playlist): Whole overlay revamp
- UI(Playlist): Fixed tracks sometimes not loading properly on scroll

# v0.6.0
- UI: Updated framework to the latest commit
- UI(Catalog): Removed unecessary padding that was causing early clipping
- UI(Playback): Updated "Repeat" and "Shuffle / Random" icons
- UI(Playlist / Sidebar): Create and modify playlist button
- UI(Track, Cover): Use skeleton while loading
- UI(Settings) + Playback: Added Loop mode (Off, Once, Track, Playlist)
- Core(Linux): Added Media Session (mpris)
- Core: Execute tasks using TaskQueue system
- Core: Added "MUSIQ_ENABLE_DEBUGGER" env var

# v0.5.0
- UI(Settings): Added interface scale option
- UI: updated framework to the latest commit

# v0.4.1
- Core(Fonts): Fix high memory usage

# v0.4.0
- UI(Sidebar): Make it resizable
- UI(Sidebar): Added Saved playlists section
- UI(Catalog): Fixed Widgets dissapearing on resize
- UI(Settings): Added "Main Font" and "Fallback Font" selectors
- UI(TrackList): Fix track only showing selected at playlist or queue lists 
- Core(Windows): Hide console window

# v0.3.0
- MPV: Enable "cache" and "cache-pause"
- Queue: Removed "AutoPlay" (recommended songs are now loaded via the "load more" button)
- Http: Reuse connection per thread (everything should load a bit faster)
- Session: Cache and restore last session data (should start a bit faster)
- Playback: Fixed song switch on network errors
- UI: Fixed broken font on some languages (Manrope -> Inter with fallback to NotoSans)
- UI(HomeOverlay): Load more sections on scroll
- UI(PlaylistOverlay): Use "play" / "pause" icons instead of text
- UI(ChannelPicker): Fixed invisible border

# v0.2.0
- Inno(Windows), Portable(Windows) and tarball(arch linux) packaging
- MIT -> GPL-3.0 license
- Implemented Random & Repeat
- Show selected track on playlist and queue list
- Fix cursor flicking on disabled items
- Fix recommended songs removing queue origin 
- Disable UI debugger on release builds

# v0.1.1
- Fix appimage packaging

# v0.1.0
## Initial release

- Home, Search and playlist browsing
- queue and radio playback through libmpv
- Login using the system webview
- Linux AppImage packaging
