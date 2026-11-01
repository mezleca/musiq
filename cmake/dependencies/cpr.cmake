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
    GIT_REPOSITORY https://github.com/libcpr/cpr
    GIT_TAG cdbbd90fb01c29dd52ed3e19f606bb4980e71541
)
FetchContent_MakeAvailable(cpr)
