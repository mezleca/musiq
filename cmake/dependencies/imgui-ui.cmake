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
    GIT_TAG 68a811c4b79c37502df7a4546aa3b90dc44a4023
)
FetchContent_MakeAvailable(imgui-ui)
