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
#include "jp2image.hpp"

#include "config.h"

#include "basicio.hpp"
#include "enforce.hpp"
#include "error.hpp"
#include "image.hpp"
#include "image_int.hpp"
#include "jp2image_int.hpp"
#include "safe_op.hpp"
#include "tiffimage.hpp"
#include "types.hpp"

#include <algorithm>
#include <array>
// *****************************************************************************
namespace Exiv2 {

namespace {
// JPEG-2000 box types
constexpr uint32_t kJp2BoxTypeSignature = 0x6a502020;    // signature box, required,
constexpr uint32_t kJp2BoxTypeFileTypeBox = 0x66747970;  // File type box, required
constexpr uint32_t kJp2BoxTypeHeader = 0x6a703268;       // Jp2 Header Box, required, Superbox
constexpr uint32_t kJp2BoxTypeImageHeader = 0x69686472;  // Image Header Box ('ihdr'), required,
constexpr uint32_t kJp2BoxTypeColorSpec = 0x636f6c72;    // Color Specification box ('colr'), required
constexpr uint32_t kJp2BoxTypeUuid = 0x75756964;         // 'uuid'
constexpr uint32_t kJp2BoxTypeClose = 0x6a703263;        // 'jp2c'

// JPEG-2000 UUIDs for embedded metadata
//
// See http://www.jpeg.org/public/wg1n2600.doc for information about embedding IPTC-NAA data in JPEG-2000 files
// See http://www.adobe.com/devnet/xmp/pdfs/xmp_specification.pdf for information about embedding XMP data in JPEG-2000
// files
constexpr unsigned char kJp2UuidExif[] = "JpgTiffExif->JP2";
constexpr unsigned char kJp2UuidIptc[] = "\x33\xc7\xa4\xd2\xb8\x1d\x47\x23\xa0\xba\xf1\xa3\xe0\x97\xad\x38";
constexpr unsigned char kJp2UuidXmp[] = "\xbe\x7a\xcf\xcb\x97\xa9\x42\xe8\x9c\x71\x99\x94\x91\xe3\xaf\xac";

// See section B.1.1 (JPEG 2000 Signature box) of JPEG-2000 specification
constexpr std::array<byte, 12> Jp2Signature{
    0x00, 0x00, 0x00, 0x0c, 0x6a, 0x50, 0x20, 0x20, 0x0d, 0x0a, 0x87, 0x0a,
};

const size_t boxHSize = sizeof(Internal::Jp2BoxHeader);

void lf(std::ostream& out, bool& bLF) {
  if (bLF) {
    out << std::endl;
    out.flush();
    bLF = false;
  }
}

bool isBigEndian() {
  union {
    uint32_t i;
    char c[4];
  } e = {0x01000000};

  return e.c[0] != 0;
}

// Obtains the ascii version from the box.type
std::string toAscii(long n) {
  const auto p = reinterpret_cast<const char*>(&n);
  std::string result;
  bool bBigEndian = isBigEndian();
  for (int i = 0; i < 4; i++) {
    result += p[bBigEndian ? i : (3 - i)];
  }
  return result;
}

void boxes_check(size_t b, size_t m) {
  if (b > m) {
#ifdef EXIV2_DEBUG_MESSAGES
    std::cout << "Exiv2::Jp2Image::readMetadata box maximum exceeded" << std::endl;
#endif
    throw Error(ErrorCode::kerCorruptedMetadata);
  }
}
}  // namespace

Jp2Image::Jp2Image(BasicIo::UniquePtr io) : Image(ImageType::jp2, mdExif | mdIptc | mdXmp, std::move(io)) {
}

std::string Jp2Image::mimeType() const {
  return "image/jp2";
}

void Jp2Image::readMetadata() {
#ifdef EXIV2_DEBUG_MESSAGES
  std::cerr << "Exiv2::Jp2Image::readMetadata: Reading JPEG-2000 file " << io_->path() << std::endl;
#endif
  if (io_->open() != 0) {
    throw Error(ErrorCode::kerDataSourceOpenFailed, io_->path(), strError());
  }
  IoCloser closer(*io_);
  if (!isJp2Type(*io_, false)) {
    throw Error(ErrorCode::kerNotAnImage, "JPEG-2000");
  }

  Internal::Jp2BoxHeader box = {0, 0};
  Internal::Jp2BoxHeader subBox = {0, 0};
  Internal::Jp2ImageHeaderBox ihdr = {0, 0, 0, 0, 0, 0, 0};
  Internal::Jp2UuidBox uuid = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
  size_t boxesCount = 0;
  const size_t boxem = 1000;  // boxes max
  uint32_t lastBoxTypeRead = 0;
  bool boxSignatureFound = false;
  bool boxFileTypeFound = false;

  while (io_->read(reinterpret_cast<byte*>(&box), boxHSize) == boxHSize) {
    boxes_check(boxesCount++, boxem);
    long position = io_->tell();
    box.length = getLong(reinterpret_cast<byte*>(&box.length), bigEndian);
    box.type = getLong(reinterpret_cast<byte*>(&box.type), bigEndian);
#ifdef EXIV2_DEBUG_MESSAGES
    std::cout << "Exiv2::Jp2Image::readMetadata: "
              << "Position: " << position << " box type: " << toAscii(box.type) << " length: " << box.length
              << std::endl;
#endif
    enforce(box.length <= boxHSize + io_->size() - io_->tell(), ErrorCode::kerCorruptedMetadata);

    if (box.length == 0)
      return;

    if (box.length == 1) {
      /// \todo In this case, the real box size is given in XLBox (bytes 8-15)
    }

    switch (box.type) {
      case kJp2BoxTypeSignature: {
        if (boxSignatureFound)  // Only one is allowed
          throw Error(ErrorCode::kerCorruptedMetadata);
        boxSignatureFound = true;
        break;
      }
      case kJp2BoxTypeFileTypeBox: {
        // This box shall immediately follow the JPEG 2000 Signature box
        if (boxFileTypeFound || lastBoxTypeRead != kJp2BoxTypeSignature) {  // Only one is allowed
          throw Error(ErrorCode::kerCorruptedMetadata);
        }
        boxFileTypeFound = true;
        std::vector<byte> boxData(box.length - boxHSize);
        io_->readOrThrow(boxData.data(), boxData.size(), ErrorCode::kerCorruptedMetadata);
        if (!Internal::isValidBoxFileType(boxData))
          throw Error(ErrorCode::kerCorruptedMetadata);
        break;
      }
      case kJp2BoxTypeHeader: {
#ifdef EXIV2_DEBUG_MESSAGES
        std::cout << "Exiv2::Jp2Image::readMetadata: JP2Header box found" << std::endl;
#endif
        long restore = io_->tell();

        while (io_->read(reinterpret_cast<byte*>(&subBox), boxHSize) == boxHSize && subBox.length) {
          boxes_check(boxesCount++, boxem);
          subBox.length = getLong(reinterpret_cast<byte*>(&subBox.length), bigEndian);
          subBox.type = getLong(reinterpret_cast<byte*>(&subBox.type), bigEndian);
          if (subBox.length > io_->size()) {
            throw Error(ErrorCode::kerCorruptedMetadata);
          }
#ifdef EXIV2_DEBUG_MESSAGES
          std::cout << "Exiv2::Jp2Image::readMetadata: "
                    << "subBox = " << toAscii(subBox.type) << " length = " << subBox.length << std::endl;
#endif
          if (subBox.type == kJp2BoxTypeColorSpec && subBox.length != 15) {
#ifdef EXIV2_DEBUG_MESSAGES
            std::cout << "Exiv2::Jp2Image::readMetadata: "
                      << "Color data found" << std::endl;
#endif

            const long pad = 3;  // 3 padding bytes 2 0 0
            const size_t data_length = Safe::add(subBox.length, 8u);
            // data_length makes no sense if it is larger than the rest of the file
            if (data_length > io_->size() - io_->tell()) {
              throw Error(ErrorCode::kerCorruptedMetadata);
            }
            DataBuf data(data_length);
            io_->read(data.data(), data.size());
            const size_t iccLength = data.read_uint32(pad, bigEndian);
            // subtracting pad from data.size() is safe:
            // data.size() is at least 8 and pad = 3
            if (iccLength > data.size() - pad) {
              throw Error(ErrorCode::kerCorruptedMetadata);
            }
            DataBuf icc(iccLength);
            std::copy_n(data.c_data(pad), icc.size(), icc.begin());
#ifdef EXIV2_DEBUG_MESSAGES
            const char* iccPath = "/tmp/libexiv2_jp2.icc";
            FILE* f = fopen(iccPath, "wb");
            if (f) {
              fwrite(icc.c_data(), icc.size(), 1, f);
              fclose(f);
            }
            std::cout << "Exiv2::Jp2Image::readMetadata: wrote iccProfile " << icc.size() << " bytes to " << iccPath
                      << std::endl;
#endif
            setIccProfile(std::move(icc));
          }

          if (subBox.type == kJp2BoxTypeImageHeader) {
            io_->read(reinterpret_cast<byte*>(&ihdr), sizeof(ihdr));
#ifdef EXIV2_DEBUG_MESSAGES
            std::cout << "Exiv2::Jp2Image::readMetadata: Ihdr data found" << std::endl;
#endif
            ihdr.imageHeight = getLong(reinterpret_cast<byte*>(&ihdr.imageHeight), bigEndian);
            ihdr.imageWidth = getLong(reinterpret_cast<byte*>(&ihdr.imageWidth), bigEndian);
            ihdr.componentCount = getShort(reinterpret_cast<byte*>(&ihdr.componentCount), bigEndian);
            enforce(ihdr.c == 7, ErrorCode::kerCorruptedMetadata);

            pixelWidth_ = ihdr.imageWidth;
            pixelHeight_ = ihdr.imageHeight;
          }

          io_->seek(restore, BasicIo::beg);
          if (io_->seek(subBox.length, BasicIo::cur) != 0) {
            throw Error(ErrorCode::kerCorruptedMetadata);
          }
          restore = io_->tell();
        }
        break;
      }

      case kJp2BoxTypeUuid: {
#ifdef EXIV2_DEBUG_MESSAGES
        std::cout << "Exiv2::Jp2Image::readMetadata: UUID box found" << std::endl;
#endif

        if (io_->read(reinterpret_cast<byte*>(&uuid), sizeof(uuid)) == sizeof(uuid)) {
          DataBuf rawData;
          size_t bufRead;
          bool bIsExif = memcmp(uuid.uuid, kJp2UuidExif, sizeof(uuid)) == 0;
          bool bIsIPTC = memcmp(uuid.uuid, kJp2UuidIptc, sizeof(uuid)) == 0;
          bool bIsXMP = memcmp(uuid.uuid, kJp2UuidXmp, sizeof(uuid)) == 0;

          if (bIsExif) {
#ifdef EXIV2_DEBUG_MESSAGES
            std::cout << "Exiv2::Jp2Image::readMetadata: Exif data found" << std::endl;
#endif
            enforce(box.length >= boxHSize + sizeof(uuid), ErrorCode::kerCorruptedMetadata);
            rawData.alloc(box.length - (boxHSize + sizeof(uuid)));
            bufRead = io_->read(rawData.data(), rawData.size());
            if (io_->error())
              throw Error(ErrorCode::kerFailedToReadImageData);
            if (bufRead != rawData.size())
              throw Error(ErrorCode::kerInputDataReadFailed);

            if (rawData.size() > 8)  // "II*\0long"
            {
              // Find the position of Exif header in bytes array.
              const char a = rawData.read_uint8(0);
              const char b = rawData.read_uint8(1);
              long pos = (a == b && (a == 'I' || a == 'M')) ? 0 : -1;

              // #1242  Forgive having Exif\0\0 in rawData.pData_
              std::array<byte, 6> exifHeader{0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
              for (size_t i = 0; pos < 0 && i < (rawData.size() - exifHeader.size()); i++) {
                if (rawData.cmpBytes(i, exifHeader.data(), exifHeader.size()) == 0) {
                  pos = static_cast<long>(i + sizeof(exifHeader));
                  EXV_WARNING << "Reading non-standard UUID-EXIF_bad box in " << io_->path() << std::endl;
                }
              }

              // If found it, store only these data at from this place.
              if (pos >= 0) {
#ifdef EXIV2_DEBUG_MESSAGES
                std::cout << "Exiv2::Jp2Image::readMetadata: Exif header found at position " << pos << std::endl;
#endif
                ByteOrder bo =
                    TiffParser::decode(exifData(), iptcData(), xmpData(), rawData.c_data(pos), rawData.size() - pos);
                setByteOrder(bo);
              }
            } else {
              EXV_WARNING << "Failed to decode Exif metadata." << std::endl;
            }
          }

          if (bIsIPTC) {
#ifdef EXIV2_DEBUG_MESSAGES
            std::cout << "Exiv2::Jp2Image::readMetadata: Iptc data found" << std::endl;
#endif
            enforce(box.length >= boxHSize + sizeof(uuid), ErrorCode::kerCorruptedMetadata);
            rawData.alloc(box.length - (boxHSize + sizeof(uuid)));
            bufRead = io_->read(rawData.data(), rawData.size());
            if (io_->error())
              throw Error(ErrorCode::kerFailedToReadImageData);
            if (bufRead != rawData.size())
              throw Error(ErrorCode::kerInputDataReadFailed);

            if (IptcParser::decode(iptcData_, rawData.c_data(), rawData.size())) {
              EXV_WARNING << "Failed to decode IPTC metadata." << std::endl;
            }
          }

          if (bIsXMP) {
            enforce(box.length >= boxHSize + sizeof(uuid), ErrorCode::kerCorruptedMetadata);
            rawData.alloc(box.length - (boxHSize + sizeof(uuid)));
            bufRead = io_->read(rawData.data(), rawData.size());
            if (io_->error())
              throw Error(ErrorCode::kerFailedToReadImageData);
            if (bufRead != rawData.size())
              throw Error(ErrorCode::kerInputDataReadFailed);
            xmpPacket_.assign(rawData.c_str(), rawData.size());

            std::string::size_type idx = xmpPacket_.find_first_of('<');
            if (idx != std::string::npos && idx > 0) {
              EXV_WARNING << "Removing " << static_cast<uint32_t>(idx)
                          << " characters from the beginning of the XMP packet" << std::endl;
              xmpPacket_ = xmpPacket_.substr(idx);
            }

            if (!xmpPacket_.empty() && XmpParser::decode(xmpData_, xmpPacket_)) {
              EXV_WARNING << "Failed to decode XMP metadata." << std::endl;
            }
          }
        }
        break;
      }

      default: {
        break;
      }
    }
    lastBoxTypeRead = box.type;

    // Move to the next box.
    io_->seek(static_cast<long>(position - boxHSize + box.length), BasicIo::beg);
    if (io_->error())
      throw Error(ErrorCode::kerFailedToReadImageData);
  }

}  // Jp2Image::readMetadata

Image::UniquePtr newJp2Instance(BasicIo::UniquePtr io, bool create) {
  auto image = std::make_unique<Jp2Image>(std::move(io));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isJp2Type(BasicIo& iIo, bool advance) {
  const int32_t len = 12;
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  bool matched = (memcmp(buf, Jp2Signature.data(), Jp2Signature.size()) == 0);
  if (!advance || !matched) {
    iIo.seek(-len, BasicIo::cur);
  }
  return matched;
}
}  // namespace Exiv2
