if(WIN32)
    FetchContent_Declare(
        sqlite3
        URL https://sqlite.org/2026/sqlite-amalgamation-3540100.zip
        URL_HASH SHA256=0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9
    )
    FetchContent_MakeAvailable(sqlite3)
    add_library(musiq-sqlite3 STATIC "${sqlite3_SOURCE_DIR}/sqlite3.c")
    target_include_directories(musiq-sqlite3 PUBLIC "${sqlite3_SOURCE_DIR}")
    add_library(SQLite::SQLite3 INTERFACE IMPORTED GLOBAL)
    target_link_libraries(SQLite::SQLite3 INTERFACE musiq-sqlite3)
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)

    # sqlite_orm calls find_package(SQLite3) even when its target already exists
    set(SQLite3_INCLUDE_DIR "${sqlite3_SOURCE_DIR}" CACHE PATH "SQLite headers" FORCE)
    set(SQLite3_LIBRARY musiq-sqlite3 CACHE STRING "SQLite target" FORCE)
endif()

find_package(SQLite3 REQUIRED)

FetchContent_Declare(
    sqlite_orm
    GIT_REPOSITORY https://github.com/fnc12/sqlite_orm
    GIT_TAG 69917bd09a84970881d755828d60d8edf79728c6
)
FetchContent_MakeAvailable(sqlite_orm)
