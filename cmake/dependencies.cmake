include(FetchContent)

if(WIN32)
    set(BUILD_SHARED_LIBS OFF)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/dependencies/boost.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/sqlite.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/imgui-ui.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/cpr.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/nlohmann-json.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/webview.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/mpv.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/miniz.cmake")
