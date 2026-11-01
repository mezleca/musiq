set(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(WEBVIEW_BUILD_STATIC_LIBRARY OFF CACHE BOOL "" FORCE)
if(LINUX)
    set(WEBVIEW_WEBKITGTK_API 4.1)
endif()

FetchContent_Declare(
    webview
    GIT_REPOSITORY https://github.com/webview/webview
    GIT_TAG f244320ab0d82fdab98a094e02996fc26d0ffd0c
)
FetchContent_MakeAvailable(webview)
