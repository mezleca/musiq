cmake_minimum_required(VERSION 3.25)
project(musiq VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED OUTPUT_NAME OR OUTPUT_NAME STREQUAL "")
    set(OUTPUT_NAME "${PROJECT_NAME}")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(cmake/dependencies.cmake)
include(CheckIPOSupported)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "/webview/(linux|windows)/")

if(WIN32)
    list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/webview/windows/impl.cpp")
else(LINUX)
    list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/webview/linux/impl.cpp")
endif()

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "src/*.hpp")

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

check_ipo_supported(RESULT MUSIQ_IPO_SUPPORTED LANGUAGES CXX)
if(MUSIQ_IPO_SUPPORTED)
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
message(STATUS "Release IPO/LTO supported: ${MUSIQ_IPO_SUPPORTED}")

option(MUSIQ_BUILD_TESTS "Build musiq tests" OFF)
if(MUSIQ_BUILD_TESTS)
    include(cmake/dependencies/catch2.cmake)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    include(CTest)
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
install(DIRECTORY resources/fonts resources/icons
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/resources
    COMPONENT Runtime
)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME} COMPONENT Runtime)

target_link_libraries(${PROJECT_NAME} PUBLIC
    Boost::process
    Boost::hash2
    OpenGL::GL
    imgui-ui::sdl
    cpr
    nlohmann_json
    webview::core
    sqlite_orm::sqlite_orm
)

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
