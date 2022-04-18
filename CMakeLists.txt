# Minimum version imposed by Centos:8
cmake_minimum_required( VERSION 3.11.0 ) 

project(wexiv          # use TWEAK to categorize the build
    VERSION 0.00.0.9   # 1.00.0    = GM               (tagged and released)
                       # 1.00.0.9  = 1.00.0 Development
                       # 1.00.0.00 = GM Preview
                       # 1.00.0.2  = RC2              (tagged and released)
                       # 1.00.0.20 = RC2 Preview
                       # 1.00.0.29 = RC2 Development
    DESCRIPTION    "Exif/IPTC/Xmp C++ metadata library and tools plus ICC Profiles."
    LANGUAGES C CXX
)

if(NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif()

include(cmake/mainSetup.cmake  REQUIRED)

# options and their default values
option( EXIV2_ENABLE_NLS              "Build native language support (requires gettext)"      OFF )

set( PACKAGE_URL                      "https://exiv2.org")

include(cmake/findDependencies.cmake   REQUIRED)
include(cmake/compilerFlags.cmake      REQUIRED)
include(cmake/generateConfigFile.cmake REQUIRED)

include_directories(${CMAKE_BINARY_DIR})  # Make the exv_conf.h file visible for the full project
include_directories(${CMAKE_BINARY_DIR}/externals/libexpat/expat)  # Make the expat_config.h file visible for the full project
include_directories(${CMAKE_BINARY_DIR}/externals/zlib)  # Make the zconf.h file visible for the full project

add_subdirectory(externals)

add_subdirectory(src)

if( EXIV2_ENABLE_NLS )
    add_subdirectory( po )
endif()

# That's all Folks!
##
