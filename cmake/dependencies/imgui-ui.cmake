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

# source archives omit submodules, fetch their pinned revisions before configuring imgui-ui
FetchContent_Declare(imgui_vendor
    URL https://codeload.github.com/ocornut/imgui/tar.gz/c1aaac65e896a45f81f47a4d68f9959e0f86c859
    URL_HASH SHA256=b3d8f99838bb3ff05e14185562345f85e00c032da4d8fc277608d7898d847114
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)

FetchContent_Declare(nsgif_vendor
    URL https://codeload.github.com/mezleca/libnsgif/tar.gz/95018a20ec825d397810826147013111ca6195be
    URL_HASH SHA256=f4005843808bb1bfb02a014bd9b3411526ed2f3c3eec021b950ab170873740b7
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)

FetchContent_MakeAvailable(imgui_vendor nsgif_vendor)
set(IMGUI_UI_IMGUI_DIR "${imgui_vendor_SOURCE_DIR}" CACHE PATH "Dear ImGui sources")

add_library(nsgif STATIC "${nsgif_vendor_SOURCE_DIR}/src/gif.c" "${nsgif_vendor_SOURCE_DIR}/src/lzw.c")
target_include_directories(nsgif PUBLIC "${nsgif_vendor_SOURCE_DIR}/include")

FetchContent_Declare(
    imgui-ui
    URL https://codeload.github.com/mezleca/imgui-ui/tar.gz/9748bff57e358c31f06dfd2564c59e22e150f6fc
    URL_HASH SHA256=1eca1d2615648e04ccd7e55aae49f485ace5966d933f64f16142da48d3e9bbbd
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(imgui-ui)
