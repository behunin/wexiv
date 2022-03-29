# wexiv
Exiv2 to Wasm

This is a stripped down version of [Exiv2](https://github.com/Exiv2/exiv2). All write/encode functions have been removed.

# Use
Example:

``` javascript
        import wexiv from '{YOUR_DIR_HERE}/wexiv.js'
        
        try {
          const tmp = new Uint8Array(arraybuffer) // arraybuffer from an image file
          const numBytes = tmp.length * tmp.BYTES_PER_ELEMENT
          const ptr = acc._malloc(numBytes)
          let heapBytes = acc.HEAPU8.subarray(ptr, ptr + numBytes)
          heapBytes.set(tmp)
          const nameBytesUTF8 = lengthBytesUTF8(file.name)
          const namePtr = acc._malloc(nameBytesUTF8)
          acc.stringToUTF8(file.name, namePtr, namePtr + nameBytesUTF8)
          if (acc._getmeta(heapBytes.byteOffset, tmp.length, namePtr) !== 0) {
            console.error("NOT Get Meta")
          }
          acc._free(heapBytes.byteOffset)
          acc._free(namePtr)
        } catch (e) {
          console.error(e)
        }
```

# Compiling

1. Follow the instuctions for [Emscripten](https://emscripten.org/docs/getting_started/downloads.html).
2. Clone this repository and enter into the wexiv directory.
3. ``git submodule init``
4. Configure the Toolchain ``emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`` in your IDE and build.

   OR 
   
   run ``cmake --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_TOOLCHAIN_FILE:FILEPATH={YOUR_DIR_HERE}/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -S{YOUR_DIR_HERE}/wexiv -B{YOUR_DIR_HERE}/wexiv/build -G "Unix Makefiles"``
`
