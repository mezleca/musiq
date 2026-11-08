find_package(OpenGL REQUIRED)

set(CMAKE_DISABLE_FIND_PACKAGE_lunasvg TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_nfd TRUE)
set(USE_SYSTEM_PLUTOVG OFF CACHE BOOL "Use bundled plutovg" FORCE)

if(WIN32)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_Freetype TRUE)
    set(CMAKE_DISABLE_FIND_PACKAGE_SDL3 TRUE)
endif()

set(IMGUI_UI_BUILD_SDL ON)
set(IMGUI_UI_BUILD_TESTS OFF)

FetchContent_Declare(
    imgui-ui
    GIT_REPOSITORY https://github.com/mezleca/imgui-ui.git
    GIT_TAG 541bc3ec5b89f3fffa9d4cd448e60c54ba03c0db
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui-ui)
