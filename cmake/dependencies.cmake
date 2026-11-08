include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled dependencies as static libraries" FORCE)

include("${CMAKE_CURRENT_LIST_DIR}/dependencies/boost.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/sqlite.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/imgui-ui.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/cpr.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/nlohmann-json.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/webview.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/mpv.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/miniz.cmake")
