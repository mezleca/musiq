# dll includes ffmpeg and other stuff
FetchContent_Declare(mpv_binary
    URL https://github.com/mpv-player/mpv/releases/download/v0.42.0/libmpv-v0.42.0-x86_64-w64-mingw32-gpl.zip
    URL_HASH SHA256=b99e0d183024ae479d1b36a93fa262c0297dfb025b99715b98da377ffb6c2913
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(mpv_binary)

add_library(mpv::mpv SHARED IMPORTED)
set_target_properties(mpv::mpv PROPERTIES
    IMPORTED_LOCATION "${mpv_binary_SOURCE_DIR}/libmpv-2.dll"
    IMPORTED_IMPLIB "${mpv_binary_SOURCE_DIR}/libmpv.dll.a"
    INTERFACE_INCLUDE_DIRECTORIES "${mpv_binary_SOURCE_DIR}/include"
)

install(IMPORTED_RUNTIME_ARTIFACTS mpv::mpv
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)

install(FILES "${mpv_binary_SOURCE_DIR}/LICENSE.GPL"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/mpv COMPONENT Runtime)
