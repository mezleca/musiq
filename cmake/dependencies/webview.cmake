set(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(WEBVIEW_BUILD_STATIC_LIBRARY OFF CACHE BOOL "" FORCE)
if(LINUX)
    set(WEBVIEW_WEBKITGTK_API 4.1)
endif()

FetchContent_Declare(
    webview
    URL https://codeload.github.com/webview/webview/tar.gz/f244320ab0d82fdab98a094e02996fc26d0ffd0c
    URL_HASH SHA256=34bb18e9fb8201a139c0e40de6df201e19e3704c91faaecaf3a1ef2f909456d9
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(webview)
