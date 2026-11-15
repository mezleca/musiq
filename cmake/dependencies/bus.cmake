if(WIN32)
elseif(LINUX)
    set(SDBUSCPP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(SDBUSCPP_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(sdbus-cpp
        GIT_REPOSITORY https://github.com/Kistler-Group/sdbus-cpp.git
        GIT_TAG v2.3.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(sdbus-cpp)

    install(FILES
        "${sdbus-cpp_SOURCE_DIR}/COPYING"
        "${sdbus-cpp_SOURCE_DIR}/COPYING-LGPL-Exception"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/sdbus-c++ COMPONENT Runtime)
endif()
