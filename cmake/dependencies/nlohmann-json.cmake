if(NOT WIN32)
    find_package(nlohmann_json 3.12 QUIET CONFIG)
    if(TARGET nlohmann_json::nlohmann_json)
        add_library(nlohmann_json ALIAS nlohmann_json::nlohmann_json)
        return()
    endif()
endif()

FetchContent_Declare(
    nlohmann_json
    URL https://codeload.github.com/nlohmann/json/tar.gz/9c349b1d7c393c6bc6dbbe4191e332e3bb4100b8
    URL_HASH SHA256=2c13007557c7a0a057c9853fdd448f7d7d0b9e7c378cafacfe4066aa798e6d26
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(nlohmann_json)
