# Unreleased

- UI(Sidebar): Make it resizable
- UI(Catalog): Fixed Widgets dissapearing on resize

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
