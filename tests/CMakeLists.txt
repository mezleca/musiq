add_executable(tests
    youtube-account.cpp
    youtube-api.cpp
    youtube-playlists.cpp
    utils.cpp
    thumbnails.cpp
    playback.cpp
    mpv-player.cpp
    ../src/playback/mpv-player.cpp
    ../src/playback/controller.cpp
    ../src/playback/audio-stream.cpp
    ../src/youtube/stream.cpp
    ../src/youtube/yt-dlp.cpp
    ../src/youtube/dlp-manager.cpp
    ../src/utils/system.cpp
    ../src/utils/random.cpp
    ../src/playback/queue.cpp
    ../src/utils/thread-pool.cpp
    ../src/app/resources/thumbnails.cpp
    ../src/utils/media.cpp
    ../src/youtube/schema/account.cpp
    ../src/youtube/schema/common.cpp
    ../src/youtube/schema/request.cpp
    ../src/youtube/schema/player.cpp
    ../src/youtube/schema/catalog.cpp
    ../src/youtube/schema/queue.cpp
    ../src/youtube/schema/continuation.cpp
    ../src/youtube/schema/playlist.cpp
    ../src/youtube/client.cpp
    ../src/youtube/catalog.cpp
    ../src/youtube/queue.cpp
    ../src/youtube/playlists.cpp
    ../src/youtube/session.cpp
    ../src/youtube/error.cpp
    ../src/http/http-client.cpp
)

target_link_libraries(tests PRIVATE
    nlohmann_json
    Catch2::Catch2WithMain
    cpr
    Boost::hash2
    Boost::process
    mpv::mpv
    miniz
    imgui-ui::sdl
)

target_compile_definitions(tests PRIVATE
    MUSIQ_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

catch_discover_tests(tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:tests>"
    DISCOVERY_MODE PRE_TEST
)
