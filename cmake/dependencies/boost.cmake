if(WIN32)
    set(BOOST_INCLUDE_LIBRARIES process hash2)
    FetchContent_Declare(
        Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-cmake.tar.xz
        URL_HASH SHA256=aca59f889f0f32028ad88ba6764582b63c916ce5f77b31289ad19421a96c555f
    )
    FetchContent_MakeAvailable(Boost)
else()
    find_package(Boost 1.88 REQUIRED CONFIG COMPONENTS process hash2)
endif()
