find_package(OpenGL REQUIRED)

set(CMAKE_DISABLE_FIND_PACKAGE_lunasvg TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_nfd TRUE)
set(USE_SYSTEM_PLUTOVG OFF CACHE BOOL "Use bundled plutovg" FORCE)

if(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/sdl-windows.cmake")
    set(CMAKE_DISABLE_FIND_PACKAGE_Freetype TRUE)
endif()

set(ARBOR_BUILD_SDL ON)
set(ARBOR_BUILD_TESTS OFF)

FetchContent_Declare(
    arbor
    GIT_REPOSITORY https://github.com/mezleca/arbor.git
    GIT_TAG 852b56aeff9e3f685c5716b08abf50b1860946a1
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(arbor)
