if(WIN32)
    FetchContent_Declare(
        SQLite3
        URL https://sqlite.org/2026/sqlite-amalgamation-3540100.zip
        URL_HASH SHA256=0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(SQLite3)
    add_library(musiq-sqlite3 STATIC "${sqlite3_SOURCE_DIR}/sqlite3.c")
    target_include_directories(musiq-sqlite3 PUBLIC "${sqlite3_SOURCE_DIR}")
    # sqlite_orm exports its target, so bundled sqlite must appear as an external dependency
    add_library(SQLite::SQLite3 INTERFACE IMPORTED GLOBAL)
    target_link_libraries(SQLite::SQLite3 INTERFACE musiq-sqlite3)
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
else()
    find_package(SQLite3 REQUIRED)
endif()

FetchContent_Declare(
    sqlite_orm
    SYSTEM
    URL https://codeload.github.com/fnc12/sqlite_orm/tar.gz/69917bd09a84970881d755828d60d8edf79728c6
    URL_HASH SHA256=39e0d489f72146c7c950cde3a91f03b6ef8dec7883c195dcbcaef75995eab035
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(sqlite_orm)
