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

#include "basicio.hpp"
#include "config.h"
#include "enforce.hpp"
#include "error.hpp"
#include "image.hpp"
#include "image_int.hpp"
#include "jp2image_int.hpp"
#include "safe_op.hpp"
#include "tiffimage.hpp"
#include "types.hpp"

// JPEG-2000 box types
const uint32_t kJp2BoxTypeJp2Header = 0x6a703268;    // 'jp2h'
const uint32_t kJp2BoxTypeImageHeader = 0x69686472;  // 'ihdr'
const uint32_t kJp2BoxTypeColorHeader = 0x636f6c72;  // 'colr'
const uint32_t kJp2BoxTypeUuid = 0x75756964;         // 'uuid'
const uint32_t kJp2BoxTypeClose = 0x6a703263;        // 'jp2c'

// from openjpeg-2.1.2/src/lib/openjp2/jp2.h
/*#define JPIP_JPIP 0x6a706970*/

#define JP2_JP 0x6a502020   /**< JPEG 2000 signature box */
#define JP2_FTYP 0x66747970 /**< File type box */
#define JP2_JP2H 0x6a703268 /**< JP2 header box (super-box) */
#define JP2_IHDR 0x69686472 /**< Image header box */
#define JP2_COLR 0x636f6c72 /**< Colour specification box */
#define JP2_JP2C 0x6a703263 /**< Contiguous codestream box */
#define JP2_URL 0x75726c20  /**< Data entry URL box */
#define JP2_PCLR 0x70636c72 /**< Palette box */
#define JP2_CMAP 0x636d6170 /**< Component Mapping box */
#define JP2_CDEF 0x63646566 /**< Channel Definition box */
#define JP2_DTBL 0x6474626c /**< Data Reference box */
#define JP2_BPCC 0x62706363 /**< Bits per component box */
#define JP2_JP2 0x6a703220  /**< File type fields */

/* For the future */
/* #define JP2_RES 0x72657320 */  /**< Resolution box (super-box) */
/* #define JP2_JP2I 0x6a703269 */ /**< Intellectual property box */
/* #define JP2_XML  0x786d6c20 */ /**< XML box */
/* #define JP2_UUID 0x75756994 */ /**< UUID box */
/* #define JP2_UINF 0x75696e66 */ /**< UUID info box (super-box) */
/* #define JP2_ULST 0x756c7374 */ /**< UUID list box */

// JPEG-2000 UUIDs for embedded metadata
//
// See http://www.jpeg.org/public/wg1n2600.doc for information about embedding IPTC-NAA data in JPEG-2000 files
// See http://www.adobe.com/devnet/xmp/pdfs/xmp_specification.pdf for information about embedding XMP data in JPEG-2000
// files
const unsigned char kJp2UuidExif[] = "JpgTiffExif->JP2";
const unsigned char kJp2UuidIptc[] = "\x33\xc7\xa4\xd2\xb8\x1d\x47\x23\xa0\xba\xf1\xa3\xe0\x97\xad\x38";
const unsigned char kJp2UuidXmp[] = "\xbe\x7a\xcf\xcb\x97\xa9\x42\xe8\x9c\x71\x99\x94\x91\xe3\xaf\xac";

// See section B.1.1 (JPEG 2000 Signature box) of JPEG-2000 specification
const unsigned char Jp2Signature[12] = {0x00, 0x00, 0x00, 0x0c, 0x6a, 0x50, 0x20, 0x20, 0x0d, 0x0a, 0x87, 0x0a};

//! @cond IGNORE
struct Jp2BoxHeader {
  uint32_t length;
  uint32_t type;
};

struct Jp2ImageHeaderBox {
  uint32_t imageHeight;
  uint32_t imageWidth;
  uint16_t componentCount;
  uint8_t bitsPerComponent;
  uint8_t compressionType;
  uint8_t colorspaceIsUnknown;
  uint8_t intellectualPropertyFlag;
  uint16_t compressionTypeProfile;
};

struct Jp2UuidBox {
  uint8_t uuid[16];
};
//! @endcond

// *****************************************************************************
namespace Exiv2 {

Jp2Image::Jp2Image(BasicIo::UniquePtr io) : Image(ImageType::jp2, mdExif | mdIptc | mdXmp, std::move(io)) {
}

std::string Jp2Image::mimeType() const {
  return "image/jp2";
}

static void boxes_check(size_t b, size_t m) {
  if (b > m) {
    throw Error(kerCorruptedMetadata);
  }
}

void Jp2Image::readMetadata() {
  if (io_->open() != 0) {
    throw Error(kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isJp2Type(*io_, true)) {
    if (io_->error() || io_->eof())
      throw Error(kerFailedToReadImageData);
    throw Error(kerNotAnImage, "JPEG-2000");
  }

  long position = 0;
  Jp2BoxHeader box = {0, 0};
  Jp2BoxHeader subBox = {0, 0};
  Jp2ImageHeaderBox ihdr = {0, 0, 0, 0, 0, 0, 0, 0};
  Jp2UuidBox uuid = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
  size_t boxes = 0;
  size_t boxem = 1000;  // boxes max

  while (io_->read(reinterpret_cast<byte*>(&box), sizeof(box)) == sizeof(box)) {
    boxes_check(boxes++, boxem);
    position = io_->tell();
    box.length = getLong(reinterpret_cast<byte*>(&box.length), bigEndian);
    box.type = getLong(reinterpret_cast<byte*>(&box.type), bigEndian);
    enforce(box.length <= sizeof(box) + io_->size() - io_->tell(), Exiv2::kerCorruptedMetadata);

    if (box.length == 0)
      return;

    if (box.length == 1) {
      // FIXME. Special case. the real box size is given in another place.
    }

    switch (box.type) {
      case kJp2BoxTypeJp2Header: {
        long restore = io_->tell();

        while (io_->read(reinterpret_cast<byte*>(&subBox), sizeof(subBox)) == sizeof(subBox) && subBox.length) {
          boxes_check(boxes++, boxem);
          subBox.length = getLong(reinterpret_cast<byte*>(&subBox.length), bigEndian);
          subBox.type = getLong(reinterpret_cast<byte*>(&subBox.type), bigEndian);
          if (subBox.length > io_->size()) {
            throw Error(kerCorruptedMetadata);
          }
          if (subBox.type == kJp2BoxTypeColorHeader && subBox.length != 15) {
            const long pad = 3;  // 3 padding bytes 2 0 0
            const size_t data_length = Safe::add(subBox.length, static_cast<uint32_t>(8));
            // data_length makes no sense if it is larger than the rest of the file
            if (data_length > io_->size() - io_->tell()) {
              throw Error(kerCorruptedMetadata);
            }
            DataBuf data(static_cast<long>(data_length));
            io_->read(data.pData_, data.size_);
            const long iccLength = getULong(data.pData_ + pad, bigEndian);
            // subtracting pad from data.size_ is safe:
            // size_ is at least 8 and pad = 3
            if (iccLength > data.size_ - pad) {
              throw Error(kerCorruptedMetadata);
            }
            DataBuf icc(iccLength);
            ::memcpy(icc.pData_, data.pData_ + pad, icc.size_);
            // setIccProfile(icc);
          }

          if (subBox.type == kJp2BoxTypeImageHeader) {
            io_->read(reinterpret_cast<byte*>(&ihdr), sizeof(ihdr));
            ihdr.imageHeight = getLong(reinterpret_cast<byte*>(&ihdr.imageHeight), bigEndian);
            ihdr.imageWidth = getLong(reinterpret_cast<byte*>(&ihdr.imageWidth), bigEndian);
            ihdr.componentCount = getShort(reinterpret_cast<byte*>(&ihdr.componentCount), bigEndian);
            ihdr.compressionTypeProfile = getShort(reinterpret_cast<byte*>(&ihdr.compressionTypeProfile), bigEndian);

            pixelWidth_ = ihdr.imageWidth;
            pixelHeight_ = ihdr.imageHeight;
          }

          io_->seek(restore, BasicIo::beg);
          if (io_->seek(subBox.length, Exiv2::BasicIo::cur) != 0) {
            throw Error(kerCorruptedMetadata);
          }
          restore = io_->tell();
        }
        break;
      }

      case kJp2BoxTypeUuid: {
        if (io_->read(reinterpret_cast<byte*>(&uuid), sizeof(uuid)) == sizeof(uuid)) {
          DataBuf rawData;
          long bufRead;
          bool bIsExif = memcmp(uuid.uuid, kJp2UuidExif, sizeof(uuid)) == 0;
          bool bIsIPTC = memcmp(uuid.uuid, kJp2UuidIptc, sizeof(uuid)) == 0;
          bool bIsXMP = memcmp(uuid.uuid, kJp2UuidXmp, sizeof(uuid)) == 0;

          if (bIsExif) {
            enforce(box.length >= sizeof(box) + sizeof(uuid), kerCorruptedMetadata);
            rawData.alloc(box.length - (sizeof(box) + sizeof(uuid)));
            bufRead = io_->read(rawData.pData_, rawData.size_);
            if (io_->error())
              throw Error(kerFailedToReadImageData);
            if (bufRead != rawData.size_)
              throw Error(kerInputDataReadFailed);

            if (rawData.size_ > 8)  // "II*\0long"
            {
              // Find the position of Exif header in bytes array.
              long pos =
                  ((rawData.pData_[0] == rawData.pData_[1]) && (rawData.pData_[0] == 'I' || rawData.pData_[0] == 'M'))
                      ? 0
                      : -1;

              // #1242  Forgive having Exif\0\0 in rawData.pData_
              const byte exifHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
              for (long i = 0; pos < 0 && i < rawData.size_ - static_cast<long>(sizeof(exifHeader)); i++) {
                if (memcmp(exifHeader, &rawData.pData_[i], sizeof(exifHeader)) == 0) {
                  pos = i + sizeof(exifHeader);
#ifndef SUPPRESS_WARNINGS
                  EXV_WARNING << "Reading non-standard UUID-EXIF_bad box in " << io_->path() << std::endl;
#endif
                }
              }

              // If found it, store only these data at from this place.
              if (pos >= 0) {
                ByteOrder bo =
                    TiffParser::decode(exifData(), iptcData(), xmpData(), rawData.pData_ + pos, rawData.size_ - pos);
                setByteOrder(bo);
              }
            } else {
#ifndef SUPPRESS_WARNINGS
              EXV_WARNING << "Failed to decode Exif metadata." << std::endl;
#endif
            }
          }

          if (bIsIPTC) {
            enforce(box.length >= sizeof(box) + sizeof(uuid), kerCorruptedMetadata);
            rawData.alloc(box.length - (sizeof(box) + sizeof(uuid)));
            bufRead = io_->read(rawData.pData_, rawData.size_);
            if (io_->error())
              throw Error(kerFailedToReadImageData);
            if (bufRead != rawData.size_)
              throw Error(kerInputDataReadFailed);

            if (IptcParser::decode(iptcData_, rawData.pData_, rawData.size_)) {
#ifndef SUPPRESS_WARNINGS
              EXV_WARNING << "Failed to decode IPTC metadata." << std::endl;
#endif
            }
          }

          if (bIsXMP) {
            enforce(box.length >= sizeof(box) + sizeof(uuid), kerCorruptedMetadata);
            rawData.alloc(box.length - static_cast<uint32_t>(sizeof(box) + sizeof(uuid)));
            bufRead = io_->read(rawData.pData_, rawData.size_);
            if (io_->error())
              throw Error(kerFailedToReadImageData);
            if (bufRead != rawData.size_)
              throw Error(kerInputDataReadFailed);
            xmpPacket_.assign(reinterpret_cast<char*>(rawData.pData_), rawData.size_);

            std::string::size_type idx = xmpPacket_.find_first_of('<');
            if (idx != std::string::npos && idx > 0) {
#ifndef SUPPRESS_WARNINGS
              EXV_WARNING << "Removing " << static_cast<uint32_t>(idx)
                          << " characters from the beginning of the XMP packet" << std::endl;
#endif
              xmpPacket_ = xmpPacket_.substr(idx);
            }

            if (!xmpPacket_.empty() && XmpParser::decode(xmpData_, xmpPacket_)) {
#ifndef SUPPRESS_WARNINGS
              EXV_WARNING << "Failed to decode XMP metadata." << std::endl;
#endif
            }
          }
        }
        break;
      }

      default: {
        break;
      }
    }

    // Move to the next box.
    io_->seek(static_cast<long>(position - sizeof(box) + box.length), BasicIo::beg);
    if (io_->error())
      throw Error(kerFailedToReadImageData);
  }

}  // Jp2Image::readMetadata

Image::UniquePtr newJp2Instance(BasicIo::UniquePtr io, bool create) {
  Image::UniquePtr image(new Jp2Image(std::move(io)));
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
  bool matched = (memcmp(buf, Jp2Signature, len) == 0);
  if (!advance || !matched) {
    iIo.seek(-len, BasicIo::cur);
  }
  return matched;
}
}  // namespace Exiv2
