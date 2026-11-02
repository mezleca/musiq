find_package(OpenGL REQUIRED)

if(WIN32)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_Freetype TRUE)
    set(CMAKE_DISABLE_FIND_PACKAGE_lunasvg TRUE)
    set(CMAKE_DISABLE_FIND_PACKAGE_nfd TRUE)
    set(CMAKE_DISABLE_FIND_PACKAGE_SDL3 TRUE)
endif()

set(IMGUI_UI_BUILD_SDL ON)
set(IMGUI_UI_BUILD_TESTS OFF)

FetchContent_Declare(
    imgui-ui
    GIT_REPOSITORY https://github.com/mezleca/imgui-ui
    GIT_TAG e4143248ca846efd0f5d39cc655bed9ee2dc4884
)
FetchContent_MakeAvailable(imgui-ui)
