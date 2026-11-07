set(CPR_CURL_USE_LIBPSL FALSE CACHE BOOL "" FORCE)
if(WIN32)
    set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
    set(CPR_FORCE_WINSSL_BACKEND ON CACHE BOOL "" FORCE)
    set(CPR_CURL_USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
else()
    set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "" FORCE)
    find_package(CURL REQUIRED)
endif()

FetchContent_Declare(
    cpr
    URL https://codeload.github.com/libcpr/cpr/tar.gz/cdbbd90fb01c29dd52ed3e19f606bb4980e71541
    URL_HASH SHA256=781368d0e9b5297c35ac9b1e511e5fb52fe01be4632a7c2bbbea369e056992c1
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(cpr)
