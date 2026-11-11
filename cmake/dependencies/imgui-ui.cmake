find_package(OpenGL REQUIRED)

set(CMAKE_DISABLE_FIND_PACKAGE_lunasvg TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_nfd TRUE)
set(USE_SYSTEM_PLUTOVG OFF CACHE BOOL "Use bundled plutovg" FORCE)

if(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/sdl-windows.cmake")
    set(CMAKE_DISABLE_FIND_PACKAGE_Freetype TRUE)
endif()

set(IMGUI_UI_BUILD_SDL ON)
set(IMGUI_UI_BUILD_TESTS OFF)

FetchContent_Declare(
    imgui-ui
    GIT_REPOSITORY https://github.com/mezleca/imgui-ui.git
    GIT_TAG 94686858e3cdc7aa22be970eb10d857d59140a98
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui-ui)
