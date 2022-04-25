# CMakeLists.txt for exiv2 library and command-line program

# Note that this is a hack for testing the internals of the library. If EXIV2_BUILD_UNIT_TESTS==OFF
# Then we only export the symbols that are explicitly exported
if( EXIV2_BUILD_UNIT_TESTS )
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON) # Requires CMake 3.3.3
endif()

include(CMakePackageConfigHelpers)

include_directories(${CMAKE_CURRENT_BINARY_DIR})


add_library( exiv2lib_int OBJECT
    canonmn_int.cpp         canonmn_int.hpp
    casiomn_int.cpp         casiomn_int.hpp
    cr2header_int.cpp       cr2header_int.hpp
    crwimage_int.cpp        crwimage_int.hpp
    fujimn_int.cpp          fujimn_int.hpp
    helper_functions.cpp    helper_functions.hpp
    image_int.cpp           image_int.hpp
    jp2image_int.cpp        jp2image_int.hpp
    makernote_int.cpp       makernote_int.hpp
    minoltamn_int.cpp       minoltamn_int.hpp
    nikonmn_int.cpp         nikonmn_int.hpp
    olympusmn_int.cpp       olympusmn_int.hpp
    orfimage_int.cpp        orfimage_int.hpp
    panasonicmn_int.cpp     panasonicmn_int.hpp
    pentaxmn_int.cpp        pentaxmn_int.hpp
    rw2image_int.cpp        rw2image_int.hpp
    safe_op.hpp             
    samsungmn_int.cpp       samsungmn_int.hpp
    sigmamn_int.cpp         sigmamn_int.hpp
    sonymn_int.cpp          sonymn_int.hpp
    tags_int.cpp            tags_int.hpp
    tiffcomposite_int.cpp   tiffcomposite_int.hpp
    tiffimage_int.cpp       tiffimage_int.hpp
    tiffvisitor_int.cpp     tiffvisitor_int.hpp
    tifffwd_int.hpp
    utils.cpp               utils.hpp
    pngchunk_int.cpp        pngchunk_int.hpp
)

set(PUBLIC_HEADERS
    ../include/exiv2/basicio.hpp
    ../include/exiv2/bmffimage.hpp
    ../include/exiv2/bmpimage.hpp
    ../include/exiv2/config.h
    ../include/exiv2/cr2image.hpp
    ../include/exiv2/crwimage.hpp
    ../include/exiv2/datasets.hpp
    ../include/exiv2/easyaccess.hpp
    ../include/exiv2/epsimage.hpp
    ../include/exiv2/error.hpp
    ../include/exiv2/exif.hpp
    ../include/exiv2/gifimage.hpp
    ../include/exiv2/image.hpp
    ../include/exiv2/iptc.hpp
    ../include/exiv2/jp2image.hpp
    ../include/exiv2/jpgimage.hpp
    ../include/exiv2/mrwimage.hpp
    ../include/exiv2/orfimage.hpp
    ../include/exiv2/photoshop.hpp
    ../include/exiv2/preview.hpp
    ../include/exiv2/pgfimage.hpp
    ../include/exiv2/pngimage.hpp
    ../include/exiv2/psdimage.hpp
    ../include/exiv2/rafimage.hpp
    ../include/exiv2/rw2image.hpp
    ../include/exiv2/slice.hpp
    ../include/exiv2/tags.hpp
    ../include/exiv2/tgaimage.hpp
    ../include/exiv2/tiffimage.hpp
    ../include/exiv2/types.hpp
    ../include/exiv2/value.hpp
    ../include/exiv2/webpimage.hpp
    ../include/exiv2/wexiv.hpp
    ../include/exiv2/xmp_exiv2.hpp
)

add_dependencies(exiv2lib_int zlib)

add_library( exiv2lib STATIC
    basicio.cpp
    bmffimage.cpp
    bmpimage.cpp
    cr2image.cpp
    crwimage.cpp
    datasets.cpp
    easyaccess.cpp
    epsimage.cpp
    error.cpp
    exif.cpp
    gifimage.cpp
    image.cpp
    iptc.cpp
    jp2image.cpp
    jpgimage.cpp
    metadatum.cpp
    mrwimage.cpp
    orfimage.cpp
    photoshop.cpp
    preview.cpp
    pgfimage.cpp
    pngimage.cpp
    psdimage.cpp
    rafimage.cpp
    rw2image.cpp
    tags.cpp
    tgaimage.cpp
    tiffimage.cpp
    types.cpp
    value.cpp
    webpimage.cpp
    xmp.cpp
    ${PUBLIC_HEADERS}
    $<TARGET_OBJECTS:exiv2lib_int>
)


# Other library target properties
# ---------------------------------------------------------

if (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
    # Do not check format overflows on this file, to skip a false positive warning
    set_source_files_properties(value.cpp PROPERTIES COMPILE_FLAGS -Wno-format-overflow)
endif()

set_target_properties( exiv2lib PROPERTIES
    VERSION       "${PROJECT_VERSION}"
    SOVERSION     "${PROJECT_VERSION_MINOR}"
    OUTPUT_NAME   exiv2
    PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMPILE_FLAGS "${EXTRA_COMPILE_FLAGS}"
)

target_link_libraries(exiv2lib PUBLIC ${XMPSDK_LIBRARY})
target_include_directories(exiv2lib PUBLIC ${XMPSDK_INCLUDE_DIR})
target_compile_definitions(exiv2lib PUBLIC EXV_ADOBE_XMPSDK=2016)

# Use include/exiv2 to build libary code
target_include_directories(exiv2lib PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/exiv2>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories(exiv2lib_int PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/exiv2>
)

target_link_libraries( exiv2lib PRIVATE zlib)

if( EXIV2_ENABLE_NLS )
    target_link_libraries(exiv2lib PRIVATE ${Intl_LIBRARIES})
    target_include_directories(exiv2lib PRIVATE ${Intl_INCLUDE_DIRS})
    target_include_directories(exiv2lib_int PRIVATE ${Intl_INCLUDE_DIRS})
    # Definition needed for translations
    join_paths(EXV_LOCALEDIR ".." "${CMAKE_INSTALL_LOCALEDIR}")
    target_compile_definitions(exiv2lib PUBLIC EXV_LOCALEDIR="${EXV_LOCALEDIR}")
endif()

if( ICONV_FOUND )
    target_link_libraries( exiv2lib PRIVATE Iconv::Iconv )
endif()

write_basic_package_version_file(exiv2ConfigVersion.cmake COMPATIBILITY ExactVersion)

# ******************************************************************************
# exiv2 application

    add_executable(wexiv wexiv.cpp)

    target_link_libraries( wexiv PRIVATE exiv2lib )

    add_dependencies(wexiv XMPCoreStatic zlib)

    target_link_options(wexiv PUBLIC
        -03
        --bind
        --no-entry
        --js-library ${PROJECT_SOURCE_DIR}/library/getmeta.js
        -sALLOW_MEMORY_GROWTH
        -sEXPORT_ES6
        -sEXPORT_NAME=wexiv
        -sEXPORTED_RUNTIME_METHODS=stringToUTF8,lengthBytesUTF8
        -sMODULARIZE
        -sSINGLE_FILE=1
        -sNO_FILESYSTEM
        -sLLD_REPORT_UNDEFINED
        -Wl,-error-limit=0
        -v
    )

    if( EXIV2_ENABLE_NLS )
        target_link_libraries(wexiv PRIVATE ${Intl_LIBRARIES})
        target_include_directories(wexiv PRIVATE ${Intl_INCLUDE_DIRS})
    endif()

# That's all Folks!
##
