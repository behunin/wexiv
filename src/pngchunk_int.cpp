// ***************************************************************** -*- C++ -*-
/*
 * Copyright (C) 2004-2021 Exiv2 authors
 * This program is part of the Exiv2 distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301 USA.
 */
// *****************************************************************************
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
#include "safe_op.hpp"
#include "tiffimage.hpp"

/*

URLs to find informations about PNG chunks :

tEXt and zTXt chunks : http://www.vias.org/pngguide/chapter11_04.html
iTXt chunk           : http://www.vias.org/pngguide/chapter11_05.html
PNG tags             : http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/PNG.html#TextualData

*/

namespace Exiv2 {
namespace Internal {

void PngChunk::decodeIHDRChunk(const DataBuf& data, int* outWidth, int* outHeight) {
  assert(data.size_ >= 8);

  // Extract image width and height from IHDR chunk.

  *outWidth = getLong(data.pData_, bigEndian);
  *outHeight = getLong(data.pData_ + 4, bigEndian);

}  // PngChunk::decodeIHDRChunk

void PngChunk::decodeTXTChunk(Image* pImage, const DataBuf& data, TxtChunkType type) {
  DataBuf key = keyTXTChunk(data);
  DataBuf arr = parseTXTChunk(data, key.size_, type);
  parseChunkContent(pImage, key.pData_, key.size_, arr);

}  // PngChunk::decodeTXTChunk

DataBuf PngChunk::decodeTXTChunk(const DataBuf& data, TxtChunkType type) {
  DataBuf key = keyTXTChunk(data);
  return parseTXTChunk(data, key.size_, type);

}  // PngChunk::decodeTXTChunk

DataBuf PngChunk::keyTXTChunk(const DataBuf& data, bool stripHeader) {
  // From a tEXt, zTXt, or iTXt chunk,
  // we get the key, it's a null terminated string at the chunk start
  const int offset = stripHeader ? 8 : 0;
  if (data.size_ <= offset)
    throw Error(kerFailedToReadImageData);
  const byte* key = data.pData_ + offset;

  // Find null string at end of key.
  int keysize = 0;
  while (key[keysize] != 0) {
    keysize++;
    // look if keysize is valid.
    if (keysize + offset >= data.size_)
      throw Error(kerFailedToReadImageData);
  }

  return DataBuf(key, keysize);

}  // PngChunk::keyTXTChunk

DataBuf PngChunk::parseTXTChunk(const DataBuf& data, int keysize, TxtChunkType type) {
  DataBuf arr;

  if (type == zTXt_Chunk) {
    enforce(data.size_ >= Safe::add(keysize, 2), Exiv2::kerCorruptedMetadata);

    // Extract a deflate compressed Latin-1 text chunk

    // we get the compression method after the key
    const byte* compressionMethod = data.pData_ + keysize + 1;
    if (*compressionMethod != 0x00) {
      // then it isn't zlib compressed and we are sunk
      throw Error(kerFailedToReadImageData);
    }

    // compressed string after the compression technique spec
    const byte* compressedText = data.pData_ + keysize + 2;
    long compressedTextSize = data.size_ - keysize - 2;
    enforce(compressedTextSize < data.size_, kerCorruptedMetadata);

    zlibUncompress(compressedText, compressedTextSize, arr);
  } else if (type == tEXt_Chunk) {
    enforce(data.size_ >= Safe::add(keysize, 1), Exiv2::kerCorruptedMetadata);
    // Extract a non-compressed Latin-1 text chunk

    // the text comes after the key, but isn't null terminated
    const byte* text = data.pData_ + keysize + 1;
    long textsize = data.size_ - keysize - 1;

    arr = DataBuf(text, textsize);
  } else if (type == iTXt_Chunk) {
    enforce(data.size_ >= Safe::add(keysize, 3), Exiv2::kerCorruptedMetadata);
    const size_t nullSeparators = std::count(&data.pData_[keysize + 3], &data.pData_[data.size_], '\0');
    enforce(nullSeparators >= 2, Exiv2::kerCorruptedMetadata);

    // Extract a deflate compressed or uncompressed UTF-8 text chunk

    // we get the compression flag after the key
    const byte compressionFlag = data.pData_[keysize + 1];
    // we get the compression method after the compression flag
    const byte compressionMethod = data.pData_[keysize + 2];

    enforce(compressionFlag == 0x00 || compressionFlag == 0x01, Exiv2::kerCorruptedMetadata);
    enforce(compressionMethod == 0x00, Exiv2::kerCorruptedMetadata);

    // language description string after the compression technique spec
    const size_t languageTextMaxSize = data.size_ - keysize - 3;
    std::string languageText = string_from_unterminated(
        reinterpret_cast<const char*>(data.pData_ + Safe::add(keysize, 3)), languageTextMaxSize);
    const size_t languageTextSize = languageText.size();

    enforce(static_cast<unsigned long>(data.size_) >=
                Safe::add(static_cast<size_t>(Safe::add(keysize, 4)), languageTextSize),
            Exiv2::kerCorruptedMetadata);
    // translated keyword string after the language description
    std::string translatedKeyText =
        string_from_unterminated(reinterpret_cast<const char*>(data.pData_ + keysize + 3 + languageTextSize + 1),
                                 data.size_ - (keysize + 3 + languageTextSize + 1));
    const auto translatedKeyTextSize = static_cast<unsigned int>(translatedKeyText.size());

    if ((compressionFlag == 0x00) || (compressionFlag == 0x01 && compressionMethod == 0x00)) {
      enforce(Safe::add(static_cast<unsigned int>(keysize + 3 + languageTextSize + 1),
                        Safe::add(translatedKeyTextSize, 1U)) <= static_cast<unsigned int>(data.size_),
              Exiv2::kerCorruptedMetadata);

      const byte* text = data.pData_ + keysize + 3 + languageTextSize + 1 + translatedKeyTextSize + 1;
      const long textsize =
          static_cast<long>(data.size_ - (keysize + 3 + languageTextSize + 1 + translatedKeyTextSize + 1));

      if (compressionFlag == 0x00) {
        // then it's an uncompressed iTXt chunk

        arr.alloc(textsize);
        arr = DataBuf(text, textsize);
      } else if (compressionFlag == 0x01 && compressionMethod == 0x00) {
        // then it's a zlib compressed iTXt chunk

        // the compressed text comes after the translated keyword, but isn't null terminated
        zlibUncompress(text, textsize, arr);
      }
    } else {
      // then it isn't zlib compressed and we are sunk
      throw Error(kerFailedToReadImageData, "Exiv2::PngChunk::parseTXTChunk: Non-standard iTXt compression method.");
    }
  } else {
    throw Error(kerFailedToReadImageData);
  }

  return arr;

}  // PngChunk::parsePngChunk

void PngChunk::parseChunkContent(Image* pImage, const byte* key, long keySize, const DataBuf& arr) {
  // We look if an ImageMagick EXIF raw profile exist.

  if (keySize >= 21 &&
      (memcmp("Raw profile type exif", key, 21) == 0 || memcmp("Raw profile type APP1", key, 21) == 0)) {
    DataBuf exifData = readRawProfile(arr, false);
    long length = exifData.size_;

    if (length > 0) {
      // Find the position of Exif header in bytes array.

      const byte exifHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
      long pos = -1;

      for (long i = 0; i < length - static_cast<long>(sizeof(exifHeader)); i++) {
        if (memcmp(exifHeader, &exifData.pData_[i], sizeof(exifHeader)) == 0) {
          pos = i;
          break;
        }
      }

      // If found it, store only these data at from this place.

      if (pos != -1) {
        pos = pos + sizeof(exifHeader);
        ByteOrder bo = TiffParser::decode(pImage->exifData(), pImage->iptcData(), pImage->xmpData(),
                                          exifData.pData_ + pos, length - pos);
        pImage->setByteOrder(bo);
      } else {
#ifndef SUPPRESS_WARNINGS
        EXV_WARNING << "Failed to decode Exif metadata.\n";
#endif
      }
    }
  }

  // We look if an ImageMagick IPTC raw profile exist.

  if (keySize >= 21 && memcmp("Raw profile type iptc", key, 21) == 0) {
    DataBuf psData = readRawProfile(arr, false);
    if (psData.size_ > 0) {
      Blob iptcBlob;
      const byte* record = nullptr;
      uint32_t sizeIptc = 0;
      uint32_t sizeHdr = 0;

      const byte* pEnd = psData.pData_ + psData.size_;
      const byte* pCur = psData.pData_;
      while (pCur < pEnd &&
             0 == Photoshop::locateIptcIrb(pCur, static_cast<long>(pEnd - pCur), &record, &sizeHdr, &sizeIptc)) {
        if (sizeIptc) {
          append(iptcBlob, record + sizeHdr, sizeIptc);
        }
        pCur = record + sizeHdr + sizeIptc;
        pCur += (sizeIptc & 1);
      }
      if (!iptcBlob.empty() &&
          IptcParser::decode(pImage->iptcData(), &iptcBlob[0], static_cast<uint32_t>(iptcBlob.size()))) {
#ifndef SUPPRESS_WARNINGS
        EXV_WARNING << "Failed to decode IPTC metadata.\n";
#endif
        pImage->clearIptcData();
      }
      // If there is no IRB, try to decode the complete chunk data
      if (iptcBlob.empty() && IptcParser::decode(pImage->iptcData(), psData.pData_, psData.size_)) {
#ifndef SUPPRESS_WARNINGS
        EXV_WARNING << "Failed to decode IPTC metadata.\n";
#endif
        pImage->clearIptcData();
      }
    }  // if (psData.size_ > 0)
  }

  // We look if an ImageMagick XMP raw profile exist.

  if (keySize >= 20 && memcmp("Raw profile type xmp", key, 20) == 0) {
    DataBuf xmpBuf = readRawProfile(arr, false);
    long length = xmpBuf.size_;

    if (length > 0) {
      std::string& xmpPacket = pImage->xmpPacket();
      xmpPacket.assign(reinterpret_cast<char*>(xmpBuf.pData_), length);
      std::string::size_type idx = xmpPacket.find_first_of('<');
      if (idx != std::string::npos && idx > 0) {
#ifndef SUPPRESS_WARNINGS
        EXV_WARNING << "Removing " << idx << " characters from the beginning of the XMP packet\n";
#endif
        xmpPacket = xmpPacket.substr(idx);
      }
      if (XmpParser::decode(pImage->xmpData(), xmpPacket)) {
#ifndef SUPPRESS_WARNINGS
        EXV_WARNING << "Failed to decode XMP metadata.\n";
#endif
      }
    }
  }

  // We look if an Adobe XMP string exist.

  if (keySize >= 17 && memcmp("XML:com.adobe.xmp", key, 17) == 0) {
    if (arr.size_ > 0) {
      std::string& xmpPacket = pImage->xmpPacket();
      xmpPacket.assign(reinterpret_cast<char*>(arr.pData_), arr.size_);
      std::string::size_type idx = xmpPacket.find_first_of('<');
      if (idx != std::string::npos && idx > 0) {
#ifndef SUPPRESS_WARNINGS
        EXV_WARNING << "Removing " << idx << " characters "
                    << "from the beginning of the XMP packet\n";
#endif
        xmpPacket = xmpPacket.substr(idx);
      }
      if (XmpParser::decode(pImage->xmpData(), xmpPacket)) {
#ifndef SUPPRESS_WARNINGS
        EXV_WARNING << "Failed to decode XMP metadata.\n";
#endif
      }
    }
  }

  // We look if a comments string exist. Note than we use only 'Description' keyword which
  // is dedicaced to store long comments. 'Comment' keyword is ignored.

  if (keySize >= 11 && memcmp("Description", key, 11) == 0 && pImage->comment().empty()) {
    // pImage->setComment(std::string(reinterpret_cast<char*>(arr.pData_), arr.size_));
  }

}  // PngChunk::parseChunkContent

void PngChunk::zlibUncompress(const byte* compressedText, unsigned int compressedTextSize, DataBuf& arr) {
  uLongf uncompressedLen = compressedTextSize * 2;  // just a starting point
  int zlibResult;
  int dos = 0;

  do {
    arr.alloc(uncompressedLen);
    zlibResult = uncompress(arr.pData_, &uncompressedLen, compressedText, compressedTextSize);
    if (zlibResult == Z_OK) {
      assert((uLongf)arr.size_ >= uncompressedLen);
      arr.size_ = uncompressedLen;
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
      throw Error(kerFailedToReadImageData);
    }
  } while (zlibResult == Z_BUF_ERROR);

  if (zlibResult != Z_OK) {
    throw Error(kerFailedToReadImageData);
  }
}  // PngChunk::zlibUncompress

DataBuf PngChunk::readRawProfile(const DataBuf& text, bool iTXt) {
  DataBuf info;
  unsigned char unhex[103] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  1,  2, 3,
                              4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15};
  if (text.size_ == 0) {
    return DataBuf();
  }

  if (iTXt) {
    info.alloc(text.size_);
    ::memcpy(info.pData_, text.pData_, text.size_);
    return info;
  }

  const char* sp = reinterpret_cast<char*>(text.pData_) + 1;            // current byte (space pointer)
  const char* eot = reinterpret_cast<char*>(text.pData_) + text.size_;  // end of text

  if (sp >= eot) {
    return DataBuf();
  }

  // Look for newline
  while (*sp != '\n') {
    sp++;
    if (sp == eot) {
      return DataBuf();
    }
  }
  sp++;  // step over '\n'
  if (sp == eot) {
    return DataBuf();
  }

  // Look for length
  while (*sp == '\0' || *sp == ' ' || *sp == '\n') {
    sp++;
    if (sp == eot) {
      return DataBuf();
    }
  }

  // Parse the length.
  long length = 0;
  while ('0' <= *sp && *sp <= '9') {
    // Compute the new length using unsigned long, so that we can
    // check for overflow.
    const unsigned long newlength = (10 * static_cast<unsigned long>(length)) + (*sp - '0');
    if (newlength > static_cast<unsigned long>(std::numeric_limits<long>::max())) {
      return DataBuf();  // Integer overflow.
    }
    length = static_cast<long>(newlength);
    sp++;
    if (sp == eot) {
      return DataBuf();
    }
  }
  sp++;  // step over '\n'
  if (sp == eot) {
    return DataBuf();
  }

  enforce(length <= (eot - sp) / 2, Exiv2::kerCorruptedMetadata);

  // Allocate space
  if (length == 0) {
    throw Error(kerInvalidMalloc,
                "Exiv2::PngChunk::readRawProfile: Unable To Copy Raw Profile: invalid profile length");
  }
  info.alloc(length);
  if (info.size_ != length) {
    throw Error(kerInvalidMalloc,
                "Exiv2::PngChunk::readRawProfile: Unable To Copy Raw Profile: cannot allocate memory");
    return DataBuf();
  }

  // Copy profile, skipping white space and column 1 "=" signs

  unsigned char* dp = info.pData_;  // decode pointer
  unsigned int nibbles = length * 2;

  for (long i = 0; i < static_cast<long>(nibbles); i++) {
    enforce(sp < eot, Exiv2::kerCorruptedMetadata);
    while (*sp < '0' || (*sp > '9' && *sp < 'a') || *sp > 'f') {
      if (*sp == '\0') {
        throw Error(kerMemoryTransferFailed,
                    "Exiv2::PngChunk::readRawProfile: Unable To Copy Raw Profile: ran out of data");
      }

      sp++;
      enforce(sp < eot, Exiv2::kerCorruptedMetadata);
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
