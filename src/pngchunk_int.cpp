// SPDX-License-Identifier: GPL-2.0-or-later

// included header files
#include "pngchunk_int.hpp"

#include "../../externals/zlib/zlib.h"  // To uncompress or compress text chunk

#include "config.h"
#include "enforce.hpp"
#include "error.hpp"
#include "exif.hpp"
#include "helper_functions.hpp"
#include "image.hpp"
#include "iptc.hpp"
#include "jpgimage.hpp"
#include "photoshop.hpp"
#include "safe_op.hpp"
#include "tiffimage.hpp"

// standard includes
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <string>

/*

URLs to find informations about PNG chunks :

tEXt and zTXt chunks : http://www.vias.org/pngguide/chapter11_04.html
iTXt chunk           : http://www.vias.org/pngguide/chapter11_05.html
PNG tags             : http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/PNG.html#TextualData

*/
namespace {
constexpr size_t nullSeparators = 2;
}

namespace Exiv2 {
namespace Internal {

void PngChunk::decodeIHDRChunk(const DataBuf& data, uint32_t* outWidth, uint32_t* outHeight) {
  // Extract image width and height from IHDR chunk.
  *outWidth = data.read_uint32(0, bigEndian);
  *outHeight = data.read_uint32(4, bigEndian);
}  // PngChunk::decodeIHDRChunk

void PngChunk::decodeTXTChunk(Image* pImage, const DataBuf& data, TxtChunkType type) {
  DataBuf key = keyTXTChunk(data);
  DataBuf arr = parseTXTChunk(data, key.size(), type);
  parseChunkContent(pImage, key.c_data(), key.size(), arr);
}  // PngChunk::decodeTXTChunk

DataBuf PngChunk::decodeTXTChunk(const DataBuf& data, TxtChunkType type) {
  DataBuf key = keyTXTChunk(data);
  return parseTXTChunk(data, key.size(), type);
}  // PngChunk::decodeTXTChunk

DataBuf PngChunk::keyTXTChunk(const DataBuf& data, bool stripHeader) {
  // From a tEXt, zTXt, or iTXt chunk,
  // we get the key, it's a null terminated string at the chunk start
  const size_t offset = stripHeader ? 8 : 0;
  if (data.size() <= offset)
    throw Error(ErrorCode::kerFailedToReadImageData);

  auto it = std::find(data.cbegin() + offset, data.cend(), 0);
  if (it == data.cend())
    throw Error(ErrorCode::kerFailedToReadImageData);

  return {data.c_data() + offset, std::distance(data.cbegin(), it) - offset};
}  // PngChunk::keyTXTChunk

DataBuf PngChunk::parseTXTChunk(const DataBuf& data, size_t keysize, TxtChunkType type) {
  DataBuf arr;

  if (type == zTXt_Chunk) {
    enforce(data.size() >= Safe::add(keysize, nullSeparators), ErrorCode::kerCorruptedMetadata);

    // Extract a deflate compressed Latin-1 text chunk

    // we get the compression method after the key
    const byte* compressionMethod = data.c_data(keysize + 1);
    if (*compressionMethod != 0x00) {
      // then it isn't zlib compressed and we are sunk
      throw Error(ErrorCode::kerFailedToReadImageData);
    }

    // compressed string after the compression technique spec
    const byte* compressedText = data.c_data(keysize + nullSeparators);
    size_t compressedTextSize = data.size() - keysize - nullSeparators;
    enforce(compressedTextSize < data.size(), ErrorCode::kerCorruptedMetadata);

    zlibUncompress(compressedText, static_cast<uint32_t>(compressedTextSize), arr);
  } else if (type == tEXt_Chunk) {
    enforce(data.size() >= Safe::add(keysize, static_cast<size_t>(1)), ErrorCode::kerCorruptedMetadata);
    // Extract a non-compressed Latin-1 text chunk

    // the text comes after the key, but isn't null terminated
    const byte* text = data.c_data(keysize + 1);
    long textsize = data.size() - keysize - 1;

    arr = DataBuf(text, textsize);
  } else if (type == iTXt_Chunk) {
    enforce(data.size() >= Safe::add(keysize, static_cast<size_t>(3)), ErrorCode::kerCorruptedMetadata);
    const size_t nullCount = std::count(data.c_data(keysize + 3), data.c_data(data.size() - 1), '\0');
    enforce(nullCount >= nullSeparators, ErrorCode::kerCorruptedMetadata);

    // Extract a deflate compressed or uncompressed UTF-8 text chunk

    // we get the compression flag after the key
    const byte compressionFlag = data.read_uint8(keysize + 1);
    // we get the compression method after the compression flag
    const byte compressionMethod = data.read_uint8(keysize + 2);

    enforce(compressionFlag == 0x00 || compressionFlag == 0x01, ErrorCode::kerCorruptedMetadata);
    enforce(compressionMethod == 0x00, ErrorCode::kerCorruptedMetadata);

    // language description string after the compression technique spec
    const size_t languageTextMaxSize = data.size() - keysize - 3;
    std::string languageText = string_from_unterminated(data.c_str(keysize + 3), languageTextMaxSize);
    const size_t languageTextSize = languageText.size();

    enforce(data.size() >= Safe::add(Safe::add(keysize, static_cast<size_t>(4)), languageTextSize),
            ErrorCode::kerCorruptedMetadata);
    // translated keyword string after the language description
    std::string translatedKeyText = string_from_unterminated(data.c_str(keysize + 3 + languageTextSize + 1),
                                                             data.size() - (keysize + 3 + languageTextSize + 1));
    const size_t translatedKeyTextSize = translatedKeyText.size();

    if ((compressionFlag == 0x00) || (compressionFlag == 0x01 && compressionMethod == 0x00)) {
      enforce(Safe::add(keysize + 3 + languageTextSize + 1, Safe::add(translatedKeyTextSize, static_cast<size_t>(1))) <=
                  data.size(),
              ErrorCode::kerCorruptedMetadata);

      const byte* text = data.c_data(keysize + 3 + languageTextSize + 1 + translatedKeyTextSize + 1);
      const long textsize =
          static_cast<long>(data.size() - (keysize + 3 + languageTextSize + 1 + translatedKeyTextSize + 1));

      if (compressionFlag == 0x00) {
        // then it's an uncompressed iTXt chunk
        arr = DataBuf(text, textsize);
      } else if (compressionFlag == 0x01 && compressionMethod == 0x00) {
        // then it's a zlib compressed iTXt chunk
        // the compressed text comes after the translated keyword, but isn't null terminated
        zlibUncompress(text, textsize, arr);
      }
    } else {
      // then it isn't zlib compressed and we are sunk
      throw Error(ErrorCode::kerFailedToReadImageData, "parseTXTChunk: Non-standard iTXt compression method.");
    }
  } else {
    throw Error(ErrorCode::kerFailedToReadImageData, "parseTXTChunk: Found a field, not expected");
  }

  return arr;

}  // PngChunk::parsePngChunk

void PngChunk::parseChunkContent(Image* pImage, const byte* key, size_t keySize, const DataBuf& arr) {
  // We look if an ImageMagick EXIF raw profile exist.

  if (keySize >= 21 &&
      (memcmp("Raw profile type exif", key, 21) == 0 || memcmp("Raw profile type APP1", key, 21) == 0)) {
    DataBuf exifData = readRawProfile(arr, false);
    size_t length = exifData.size();

    if (length >= 6) {  // length should have at least the size of exifHeader
      // Find the position of Exif header in bytes array.
      const std::array<byte, 6> exifHeader{0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
      size_t pos = std::numeric_limits<size_t>::max();

      /// \todo Find substring inside an string
      for (size_t i = 0; i < length - exifHeader.size(); i++) {
        if (exifData.cmpBytes(i, exifHeader.data(), exifHeader.size()) == 0) {
          pos = i;
          break;
        }
      }

      // If found it, store only these data at from this place.

      if (pos != std::numeric_limits<size_t>::max()) {
        pos = pos + sizeof(exifHeader);
        ByteOrder bo = TiffParser::decode(pImage->exifData(), pImage->iptcData(), pImage->xmpData(),
                                          exifData.c_data(pos), length - pos);
        pImage->setByteOrder(bo);
      } else {
        EXV_WARNING << "pngChunk: Failed to decode Exif metadata.\n";
      }
    }
  }

  // We look if an ImageMagick IPTC raw profile exist.

  if (keySize >= 21 && memcmp("Raw profile type iptc", key, 21) == 0) {
    DataBuf psData = readRawProfile(arr, false);
    if (!psData.empty()) {
      Blob iptcBlob;
      const byte* record = nullptr;
      uint32_t sizeIptc = 0;
      uint32_t sizeHdr = 0;

      const byte* pEnd = psData.c_data(psData.size() - 1);
      const byte* pCur = psData.c_data();
      while (pCur < pEnd && 0 == Photoshop::locateIptcIrb(pCur, pEnd - pCur, &record, &sizeHdr, &sizeIptc)) {
        if (sizeIptc) {
          append(iptcBlob, record + sizeHdr, sizeIptc);
        }
        pCur = record + sizeHdr + sizeIptc;
        pCur += (sizeIptc & 1);
      }
      if (!iptcBlob.empty() && IptcParser::decode(pImage->iptcData(), &iptcBlob[0], iptcBlob.size())) {
        EXV_WARNING << "pngChunk: Failed to decode IPTC metadata.\n";
        pImage->clearIptcData();
      }
      // If there is no IRB, try to decode the complete chunk data
      if (iptcBlob.empty() && IptcParser::decode(pImage->iptcData(), psData.c_data(), psData.size())) {
        EXV_WARNING << "pngChunk: Failed to decode IPTC metadata.\n";
        pImage->clearIptcData();
      }
    }  // if (psData.size_ > 0)
  }

  // We look if an ImageMagick XMP raw profile exist.

  if (keySize >= 20 && memcmp("Raw profile type xmp", key, 20) == 0) {
    DataBuf xmpBuf = readRawProfile(arr, false);
    size_t length = xmpBuf.size();

    if (length > 0) {
      std::string& xmpPacket = pImage->xmpPacket();
      xmpPacket.assign(xmpBuf.c_str(), length);
      std::string::size_type idx = xmpPacket.find_first_of('<');
      if (idx != std::string::npos && idx > 0) {
        EXV_WARNING << "pngChunk: Removing " << idx << " characters from the beginning of the XMP packet\n";
        xmpPacket = xmpPacket.substr(idx);
      }
      if (XmpParser::decode(pImage->xmpData(), xmpPacket)) {
        EXV_WARNING << "pngChunk: Failed to decode XMP metadata.\n";
      }
    }
  }

  // We look if an Adobe XMP string exist.

  if (keySize >= 17 && memcmp("XML:com.adobe.xmp", key, 17) == 0) {
    if (!arr.empty()) {
      std::string& xmpPacket = pImage->xmpPacket();
      xmpPacket.assign(arr.c_str(), arr.size());
      std::string::size_type idx = xmpPacket.find_first_of('<');
      if (idx != std::string::npos && idx > 0) {
        EXV_WARNING << "pngChunk: Removing " << idx << " characters "
                    << "from the beginning of the XMP packet\n";
        xmpPacket = xmpPacket.substr(idx);
      }
      if (XmpParser::decode(pImage->xmpData(), xmpPacket)) {
        EXV_WARNING << "pngChunk: Failed to decode XMP metadata.\n";
      }
    }
  }

  // We look if a comments string exist. Note than we use only 'Description' keyword which
  // is dedicaced to store long comments. 'Comment' keyword is ignored.

  if (keySize >= 11 && memcmp("Description", key, 11) == 0 && pImage->comment().empty()) {
    pImage->setComment(std::string(arr.c_str(), arr.size()));
  }

}  // PngChunk::parseChunkContent

void PngChunk::zlibUncompress(const byte* compressedText, unsigned int compressedTextSize, DataBuf& arr) {
  uLongf uncompressedLen = compressedTextSize * 2;  // just a starting point
  int zlibResult;
  int dos = 0;

  do {
    arr.alloc(uncompressedLen);
    zlibResult = uncompress(arr.data(), &uncompressedLen, compressedText, compressedTextSize);
    if (zlibResult == Z_OK) {
      arr.resize(uncompressedLen);
    } else if (zlibResult == Z_BUF_ERROR) {
      // the uncompressedArray needs to be larger
      uncompressedLen *= 2;
      // DoS protection. can't be bigger than 64k
      if (uncompressedLen > 131072) {
        if (++dos > 1)
          break;
        uncompressedLen = 131072;
      }
    } else {
      // something bad happened
      throw Error(ErrorCode::kerFailedToReadImageData);
    }
  } while (zlibResult == Z_BUF_ERROR);

  if (zlibResult != Z_OK) {
    throw Error(ErrorCode::kerFailedToReadImageData);
  }
}  // PngChunk::zlibUncompress

DataBuf PngChunk::readRawProfile(const DataBuf& text, bool iTXt) {
  if (text.size() <= 1) {
    return {};
  }

  DataBuf info;
  unsigned char unhex[103] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  1,  2, 3,
                              4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15};

  if (iTXt) {
    info.alloc(text.size());
    std::copy(text.cbegin(), text.cend(), info.begin());
    return info;
  }

  const char* sp = text.c_str(1);                 // current byte (space pointer)
  const char* eot = text.c_str(text.size() - 1);  // end of text

  if (sp >= eot) {
    return {};
  }

  // Look for newline
  while (*sp != '\n') {
    sp++;
    if (sp == eot) {
      return {};
    }
  }
  sp++;  // step over '\n'
  if (sp == eot) {
    return {};
  }

  // Look for length
  while (*sp == '\0' || *sp == ' ' || *sp == '\n') {
    sp++;
    if (sp == eot) {
      return {};
    }
  }

  // Parse the length.
  size_t length = 0;
  while ('0' <= *sp && *sp <= '9') {
    // Compute the new length using unsigned long, so that we can check for overflow.
    const size_t newlength = (10 * length) + (*sp - '0');
    if (newlength > std::numeric_limits<size_t>::max()) {
      return {};  // Integer overflow.
    }
    length = newlength;
    sp++;
    if (sp == eot) {
      return {};
    }
  }
  sp++;  // step over '\n'
  if (sp == eot) {
    return {};
  }

  enforce(length <= static_cast<size_t>(eot - sp) / 2, ErrorCode::kerCorruptedMetadata);

  // Allocate space
  if (length == 0) {
    EXV_ERROR << "PngChunk::readRawProfile: Unable To Copy Raw Profile: invalid profile length";
  }
  info.alloc(length);
  if (info.size() != length) {
    EXV_ERROR << "PngChunk::readRawProfile: Unable To Copy Raw Profile: cannot allocate memory";
    return {};
  }

  // Copy profile, skipping white space and column 1 "=" signs

  unsigned char* dp = info.data();  // decode pointer
  size_t nibbles = length * 2;

  for (size_t i = 0; i < nibbles; i++) {
    enforce(sp < eot, Exiv2::ErrorCode::kerCorruptedMetadata);
    while (*sp < '0' || (*sp > '9' && *sp < 'a') || *sp > 'f') {
      if (*sp == '\0') {
        EXV_ERROR << "Exiv2::PngChunk::readRawProfile: Unable To Copy Raw Profile: ran out of data\n";
        return {};
      }

      sp++;
      enforce(sp < eot, ErrorCode::kerCorruptedMetadata);
    }

    if (i % 2 == 0)
      *dp = static_cast<unsigned char>(16 * unhex[static_cast<int>(*sp++)]);
    else
      (*dp++) += unhex[static_cast<int>(*sp++)];
  }

  return info;
}  // PngChunk::readRawProfile

}  // namespace Internal
}  // namespace Exiv2
