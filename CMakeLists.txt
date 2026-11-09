cmake_minimum_required(VERSION 3.29)
project(musiq VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(CMAKE_LINKER_TYPE LLD)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(GNUInstallDirs)
include(cmake/dependencies.cmake)
include(CheckIPOSupported)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "/webview/(linux|windows)/")

if(WIN32)
    list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/webview/windows/impl.cpp")
else()
    list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/webview/linux/impl.cpp")
endif()

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "src/*.hpp")

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

check_ipo_supported(RESULT MUSIQ_IPO_SUPPORTED LANGUAGES CXX)
if(MUSIQ_IPO_SUPPORTED)
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
message(STATUS "Release IPO/LTO supported: ${MUSIQ_IPO_SUPPORTED}")

file(RELATIVE_PATH MUSIQ_INSTALLED_RESOURCE_ROOT
    "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")
target_compile_definitions(${PROJECT_NAME} PRIVATE MUSIQ_INSTALLED_RESOURCE_ROOT="${MUSIQ_INSTALLED_RESOURCE_ROOT}")

option(MUSIQ_BUILD_TESTS "Build musiq tests" OFF)
if(MUSIQ_BUILD_TESTS)
    include(cmake/dependencies/catch2.cmake)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    include(CTest)
    add_subdirectory(tests)
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
install(DIRECTORY resources/fonts
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/resources
    COMPONENT Runtime
)

install(DIRECTORY resources/icons
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/resources
    COMPONENT Runtime
    FILES_MATCHING PATTERN "*.svg" PATTERN "musiq.png"
)

install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME} COMPONENT Runtime)

if(LINUX)
    install(FILES resources/packaging/org.mezleca.musiq.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT Runtime)
    install(FILES resources/icons/app/musiq.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME org.mezleca.musiq.png COMPONENT Runtime)
endif()

include(cmake/packaging.cmake)

target_link_libraries(${PROJECT_NAME} PUBLIC
    Boost::process
    Boost::hash2
    OpenGL::GL
    imgui-ui::sdl
    cpr
    nlohmann_json
    webview::core
    sqlite_orm::sqlite_orm
    mpv::mpv
    miniz
)

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS VERBATIM)
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8 /Zc:preprocessor)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic -Wunused-variable -Wunused-function
        $<$<CXX_COMPILER_ID:Clang>:-Wdeprecated-copy-with-user-provided-copy -Wunused-lambda-capture>
        $<$<CONFIG:Debug>:-O0 -g3>
    )
endif()
