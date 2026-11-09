FetchContent_Declare(sdl_binary
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-devel-3.4.18-VC.zip
    URL_HASH SHA256=78d84602ae616cfe26b33a73b7d3b9b6b56e8a6650e53d92c9c6ca938f747acd
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(sdl_binary)

find_package(SDL3 3.4.18 EXACT REQUIRED CONFIG COMPONENTS SDL3-shared
    PATHS "${sdl_binary_SOURCE_DIR}/cmake" NO_DEFAULT_PATH)

install(IMPORTED_RUNTIME_ARTIFACTS SDL3::SDL3-shared
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)

install(FILES "${sdl_binary_SOURCE_DIR}/LICENSE.txt"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SDL3 COMPONENT Runtime)
