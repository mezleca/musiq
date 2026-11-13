file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER TEST_SOURCES EXCLUDE REGEX "/src/main\\.cpp$|/webview/(linux|windows)/")

if(WIN32)
    list(APPEND TEST_SOURCES "${PROJECT_SOURCE_DIR}/src/webview/windows/impl.cpp")
else()
    list(APPEND TEST_SOURCES "${PROJECT_SOURCE_DIR}/src/webview/linux/impl.cpp")
endif()

add_executable(tests ${TEST_SOURCES})

target_link_libraries(tests PRIVATE
    nlohmann_json
    Catch2::Catch2WithMain
    cpr
    Boost::hash2
    Boost::process
    mpv::mpv
    miniz
    arbor::sdl
    OpenGL::GL
    webview::core
    sqlite_orm::sqlite_orm
)

target_compile_definitions(tests PRIVATE
    MUSIQ_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    MUSIQ_INSTALLED_RESOURCE_ROOT="${MUSIQ_INSTALLED_RESOURCE_ROOT}"
)

if(MSVC)
    target_compile_options(tests PRIVATE /utf-8 /Zc:preprocessor)
    target_compile_definitions(tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(WIN32)
    add_custom_command(TARGET tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:tests> $<TARGET_FILE_DIR:tests>
        COMMAND_EXPAND_LISTS VERBATIM)
endif()

catch_discover_tests(tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:tests>"
    DISCOVERY_MODE PRE_TEST
)
