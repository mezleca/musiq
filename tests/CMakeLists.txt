add_executable(tests
    youtube-account.cpp
    youtube-api.cpp
    utils.cpp
    ../src/app/resources/thumbnails.cpp
    ../src/utils/thread-pool.cpp
    ../src/youtube/schema/account.cpp
    ../src/youtube/config.cpp
    ../src/youtube/schema/common.cpp
    ../src/youtube/schema/request.cpp
    ../src/youtube/schema/player.cpp
    ../src/youtube/schema/catalog.cpp
    ../src/youtube/schema/queue.cpp
    ../src/youtube/schema/continuation.cpp
    ../src/youtube/client.cpp
    ../src/youtube/catalog.cpp
    ../src/youtube/queue.cpp
    ../src/youtube/session.cpp
    ../src/youtube/error.cpp
    ../src/http/http-client.cpp
)

target_link_libraries(tests PRIVATE
    nlohmann_json
    Catch2::Catch2WithMain
    cpr
    Boost::hash2
)

target_compile_definitions(tests PRIVATE
    MUSIQ_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

catch_discover_tests(tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:tests>"
    DISCOVERY_MODE PRE_TEST
)
