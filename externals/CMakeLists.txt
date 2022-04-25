add_subdirectory(zlib)

set(XMP_BUILD_STATIC True)
add_subdirectory(xmp/XMPCore/build)

add_dependencies(XMPCoreStatic zlib)