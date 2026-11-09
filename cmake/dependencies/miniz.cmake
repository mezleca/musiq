if(NOT WIN32)
    find_package(miniz 3 QUIET CONFIG)
    if(TARGET miniz::miniz)
        add_library(miniz ALIAS miniz::miniz)
        return()
    endif()
endif()

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(miniz
    SYSTEM
    URL https://codeload.github.com/richgel999/miniz/tar.gz/3.1.0
    URL_HASH SHA256=09569fc19d060ac9f5999ba9356728c2494ebe6a24ac0eb0a6b6ae3d396cfea6
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(miniz)

# vendored dependency, do not surface its warnings in the build or clangd
if(NOT MSVC)
    target_compile_options(miniz PRIVATE -w)
endif()
