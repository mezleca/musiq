if(CMAKE_CONFIGURATION_TYPES)
    message(FATAL_ERROR "The static mpv build requires a single-config generator, use Ninja.")
endif()
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Static libmpv requires Clang on Windows, configure Musiq with clang-cl or clang++.")
endif()

find_program(MUSIQ_MESON meson REQUIRED)
find_program(PKG_CONFIG_EXECUTABLE NAMES pkgconf pkg-config REQUIRED)

FetchContent_Declare(mpv_source
    URL https://codeload.github.com/mpv-player/mpv/tar.gz/v0.41.0
    URL_HASH SHA256=ee21092a5ee427353392360929dc64645c54479aefdb5babc5cfbb5fad626209
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(mpv_source)

string(TOLOWER "${CMAKE_BUILD_TYPE}" mpv_build_type)
set(mpv_prefix "${CMAKE_BINARY_DIR}/_deps/mpv-install-${mpv_build_type}")
set(mpv_build "${CMAKE_BINARY_DIR}/_deps/mpv-build-${mpv_build_type}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/mpv-subprojects/" DESTINATION "${mpv_source_SOURCE_DIR}/subprojects")

function(musiq_mpv_build stage)
    execute_process(COMMAND ${ARGN}
        WORKING_DIRECTORY "${mpv_source_SOURCE_DIR}"
        RESULT_VARIABLE result
        OUTPUT_FILE "${CMAKE_BINARY_DIR}/mpv-${stage}.log"
        ERROR_FILE "${CMAKE_BINARY_DIR}/mpv-${stage}.log"
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "mpv ${stage} failed, see ${CMAKE_BINARY_DIR}/mpv-${stage}.log")
    endif()
endfunction()

set(mpv_compilers "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}")
set(mpv_runtime)
if(MSVC)
    set(mpv_compilers CC=clang CXX=clang++ CC_LD=lld-link CXX_LD=lld-link)
    set(mpv_runtime -Db_vscrt=mt)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(mpv_runtime -Db_vscrt=mtd)
    endif()
endif()

# pkg-config needs the installed static dependency metadata during cmake configuration
musiq_mpv_build(configure "${CMAKE_COMMAND}" -E env ${mpv_compilers}
    "${MUSIQ_MESON}" setup --reconfigure "${mpv_build}"
    --prefix "${mpv_prefix}" --libdir lib --buildtype "${mpv_build_type}"
    --default-library static --wrap-mode=forcefallback -Dprefer_static=true -Dauto_features=disabled ${mpv_runtime}
    -Dlibmpv=true -Dcplayer=false -Dwasapi=enabled -Dwin32-threads=enabled -Dgl=disabled -Dlua=disabled
    -Dlibass:require-system-font-provider=false -Dlibplacebo:demos=false
    -Dffmpeg:auto_features=auto -Dffmpeg:network=disabled
    -Dffmpeg:programs=disabled -Dffmpeg:tests=disabled
)
musiq_mpv_build(compile "${MUSIQ_MESON}" compile -C "${mpv_build}")
musiq_mpv_build(install "${MUSIQ_MESON}" install -C "${mpv_build}")

set(mpv_old_pkg_path "$ENV{PKG_CONFIG_PATH}")
set(ENV{PKG_CONFIG_PATH} "${mpv_prefix}/lib/pkgconfig;${mpv_prefix}/share/pkgconfig")
block(SCOPE_FOR VARIABLES)
    find_package(PkgConfig REQUIRED)
    set(PKG_CONFIG_ARGN --static)
    list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES .a)
    pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
endblock()
set(ENV{PKG_CONFIG_PATH} "${mpv_old_pkg_path}")
add_library(mpv::mpv ALIAS PkgConfig::MPV)
