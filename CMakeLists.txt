# Minimum version imposed by Centos:8
cmake_minimum_required( VERSION 3.21.0 ) 

project(wexiv          # use TWEAK to categorize the build
    VERSION 0.00.0.9   # 1.00.0    = GM               (tagged and released)
                       # 1.00.0.9  = 1.00.0 Development
                       # 1.00.0.00 = GM Preview
                       # 1.00.0.2  = RC2              (tagged and released)
                       # 1.00.0.20 = RC2 Preview
                       # 1.00.0.29 = RC2 Development
    DESCRIPTION    "Exif/IPTC/Xmp C++ metadata library and tools plus ICC Profiles, Resizing."
    LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif()

include(cmake/mainSetup.cmake  REQUIRED)

# options and their default values
option( BUILD_SHARED_LIBS             "Build exiv2lib as a shared library"                    ON  )
option( EXIV2_ENABLE_XMP              "Build with XMP metadata support"                       ON  )
option( EXIV2_ENABLE_EXTERNAL_XMP     "Use external version of XMP"                           OFF )
option( EXIV2_ENABLE_PNG              "Build with png support (requires libz)"                ON  )
option( EXIV2_ENABLE_NLS              "Build native language support (requires gettext)"      OFF )
option( EXIV2_ENABLE_PRINTUCS2        "Build with Printucs2"                                  ON  )
option( EXIV2_ENABLE_LENSDATA         "Build including lens data"                             ON  )
option( EXIV2_ENABLE_DYNAMIC_RUNTIME  "Use dynamic runtime (used for static libs)"            ON  )
option( EXIV2_ENABLE_WIN_UNICODE      "Use Unicode paths (wstring) on Windows"                OFF )
option( EXIV2_ENABLE_WEBREADY         "Build webready support into library"                   OFF )
option( EXIV2_ENABLE_CURL             "USE Libcurl for HttpIo (WEBREADY)"                     OFF )
option( EXIV2_ENABLE_BMFF             "Build with BMFF support"                               ON  )

option( EXIV2_BUILD_SAMPLES           "Build sample applications"                             OFF )
option( EXIV2_BUILD_EXIV2_COMMAND     "Build exiv2 command-line executable"                   OFF )
option( EXIV2_BUILD_UNIT_TESTS        "Build unit tests"                                      OFF )
option( EXIV2_BUILD_FUZZ_TESTS        "Build fuzz tests (libFuzzer)"                          OFF )
option( EXIV2_BUILD_DOC               "Add 'doc' target to generate documentation"            OFF )

# Only intended to be used by Exiv2 developers/contributors
option( EXIV2_TEAM_EXTRA_WARNINGS     "Add more sanity checks using compiler flags"           OFF )
option( EXIV2_TEAM_WARNINGS_AS_ERRORS "Treat warnings as errors"                              OFF )
option( EXIV2_TEAM_USE_SANITIZERS     "Enable ASAN and UBSAN when available"                  OFF )
option( EXIV2_TEAM_PACKAGING          "Additional stuff for generating packages"              OFF )
set(EXTRA_COMPILE_FLAGS " ")

mark_as_advanced(
    EXIV2_TEAM_EXTRA_WARNINGS
    EXIV2_TEAM_WARNINGS_AS_ERRORS
    EXIV2_ENABLE_EXTERNAL_XMP
    EXTRA_COMPILE_FLAGS
    EXIV2_TEAM_USE_SANITIZERS
)

option( BUILD_WITH_STACK_PROTECTOR    "Build with stack protector"                            OFF )
option( BUILD_WITH_CCACHE             "Use ccache to speed up compilations"                   OFF )
option( BUILD_WITH_COVERAGE           "Add compiler flags to generate coverage stats"         OFF )

set( PACKAGE_URL                      "https://exiv2.org")

include(cmake/findDependencies.cmake   REQUIRED)
include(cmake/compilerFlags.cmake      REQUIRED)
include(cmake/generateConfigFile.cmake REQUIRED)

include_directories(${CMAKE_BINARY_DIR})  # Make the exv_conf.h file visible for the full project
include_directories(${CMAKE_BINARY_DIR}/externals/libexpat/expat)  # Make the expat_config.h file visible for the full project
include_directories(${CMAKE_BINARY_DIR}/externals/zlib)  # Make the zconf.h file visible for the full project

# include(cmake/compilerFlagsExiv2.cmake REQUIRED)

add_subdirectory(externals)

add_subdirectory(src)

if( EXIV2_ENABLE_NLS )
    add_subdirectory( po )
endif()

# That's all Folks!
##
