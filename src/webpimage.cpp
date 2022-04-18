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
/*
  Google's WEBP container spec can be found at the link below:
  https://developers.google.com/speed/webp/docs/riff_container
*/

// *****************************************************************************
// included header files
#include "webpimage.hpp"

#include "basicio.hpp"
#include "config.h"
#include "enforce.hpp"
#include "image_int.hpp"
#include "safe_op.hpp"
#include "tags.hpp"
#include "tags_int.hpp"
#include "tiffimage.hpp"
#include "tiffimage_int.hpp"
#include "types.hpp"

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))

// *****************************************************************************
// class member definitions
namespace Exiv2 {
using namespace Exiv2::Internal;

// This static function is a temporary fix in v0.27. In the next version,
// it will be added as a method of BasicIo.
static void readOrThrow(BasicIo& iIo, byte* buf, long rcount, ErrorCode err) {
  const long nread = iIo.read(buf, rcount);
  enforce(nread == rcount, err);
  enforce(!iIo.error(), err);
}

WebPImage::WebPImage(BasicIo::UniquePtr io) : Image(ImageType::webp, mdNone, std::move(io)) {
}  // WebPImage::WebPImage

std::string WebPImage::mimeType() const {
  return "image/webp";
}

/* =========================================== */

/* Misc. */
constexpr byte WebPImage::WEBP_PAD_ODD = 0;
constexpr int WebPImage::WEBP_TAG_SIZE = 0x4;

/* VP8X feature flags */
constexpr int WebPImage::WEBP_VP8X_ICC_BIT = 0x20;
constexpr int WebPImage::WEBP_VP8X_ALPHA_BIT = 0x10;
constexpr int WebPImage::WEBP_VP8X_EXIF_BIT = 0x8;
constexpr int WebPImage::WEBP_VP8X_XMP_BIT = 0x4;

/* Chunk header names */
constexpr const char* WebPImage::WEBP_CHUNK_HEADER_VP8X = "VP8X";
constexpr const char* WebPImage::WEBP_CHUNK_HEADER_VP8L = "VP8L";
constexpr const char* WebPImage::WEBP_CHUNK_HEADER_VP8 = "VP8 ";
constexpr const char* WebPImage::WEBP_CHUNK_HEADER_ANMF = "ANMF";
constexpr const char* WebPImage::WEBP_CHUNK_HEADER_ANIM = "ANIM";
constexpr const char* WebPImage::WEBP_CHUNK_HEADER_ICCP = "ICCP";
constexpr const char* WebPImage::WEBP_CHUNK_HEADER_EXIF = "EXIF";
constexpr const char* WebPImage::WEBP_CHUNK_HEADER_XMP = "XMP ";

/* =========================================== */

void WebPImage::readMetadata() {
  if (io_->open() != 0)
    throw Error(kerDataSourceOpenFailed, io_->path());
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isWebPType(*io_, true)) {
    if (io_->error() || io_->eof())
      throw Error(kerFailedToReadImageData);
    throw Error(kerNotAJpeg);
  }

  byte data[12];
  DataBuf chunkId(5);
  chunkId.pData_[4] = '\0';

  readOrThrow(*io_, data, WEBP_TAG_SIZE * 3, Exiv2::kerCorruptedMetadata);

  const uint32_t filesize_u32 = Safe::add(Exiv2::getULong(data + WEBP_TAG_SIZE, littleEndian), 8U);
  enforce(filesize_u32 <= io_->size(), Exiv2::kerCorruptedMetadata);

  // Check that `filesize_u32` is safe to cast to `long`.
  enforce(filesize_u32 <= static_cast<size_t>(std::numeric_limits<unsigned int>::max()), Exiv2::kerCorruptedMetadata);

  WebPImage::decodeChunks(static_cast<long>(filesize_u32));

}  // WebPImage::readMetadata

void WebPImage::decodeChunks(long filesize) {
  DataBuf chunkId(5);
  byte size_buff[WEBP_TAG_SIZE];
  bool has_canvas_data = false;

  chunkId.pData_[4] = '\0';
  while (!io_->eof() && io_->tell() < filesize) {
    readOrThrow(*io_, chunkId.pData_, WEBP_TAG_SIZE, Exiv2::kerCorruptedMetadata);
    readOrThrow(*io_, size_buff, WEBP_TAG_SIZE, Exiv2::kerCorruptedMetadata);

    const uint32_t size_u32 = Exiv2::getULong(size_buff, littleEndian);

    // Check that `size_u32` is safe to cast to `long`.
    enforce(size_u32 <= static_cast<size_t>(std::numeric_limits<unsigned int>::max()), Exiv2::kerCorruptedMetadata);
    const long size = static_cast<long>(size_u32);

    // Check that `size` is within bounds.
    enforce(io_->tell() <= filesize, Exiv2::kerCorruptedMetadata);
    enforce(size <= (filesize - io_->tell()), Exiv2::kerCorruptedMetadata);

    DataBuf payload(size);

    if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8X) && !has_canvas_data) {
      enforce(size >= 10, Exiv2::kerCorruptedMetadata);

      has_canvas_data = true;
      byte size_buf[WEBP_TAG_SIZE];

      readOrThrow(*io_, payload.pData_, payload.size_, Exiv2::kerCorruptedMetadata);

      // Fetch width
      memcpy(&size_buf, &payload.pData_[4], 3);
      size_buf[3] = 0;
      pixelWidth_ = Exiv2::getULong(size_buf, littleEndian) + 1;

      // Fetch height
      memcpy(&size_buf, &payload.pData_[7], 3);
      size_buf[3] = 0;
      pixelHeight_ = Exiv2::getULong(size_buf, littleEndian) + 1;
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8) && !has_canvas_data) {
      enforce(size >= 10, Exiv2::kerCorruptedMetadata);

      has_canvas_data = true;
      readOrThrow(*io_, payload.pData_, payload.size_, Exiv2::kerCorruptedMetadata);
      byte size_buf[WEBP_TAG_SIZE];

      // Fetch width""
      memcpy(&size_buf, &payload.pData_[6], 2);
      size_buf[2] = 0;
      size_buf[3] = 0;
      pixelWidth_ = Exiv2::getULong(size_buf, littleEndian) & 0x3fff;

      // Fetch height
      memcpy(&size_buf, &payload.pData_[8], 2);
      size_buf[2] = 0;
      size_buf[3] = 0;
      pixelHeight_ = Exiv2::getULong(size_buf, littleEndian) & 0x3fff;
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8L) && !has_canvas_data) {
      enforce(size >= 5, Exiv2::kerCorruptedMetadata);

      has_canvas_data = true;
      byte size_buf_w[2];
      byte size_buf_h[3];

      readOrThrow(*io_, payload.pData_, payload.size_, Exiv2::kerCorruptedMetadata);

      // Fetch width
      memcpy(&size_buf_w, &payload.pData_[1], 2);
      size_buf_w[1] &= 0x3F;
      pixelWidth_ = Exiv2::getUShort(size_buf_w, littleEndian) + 1;

      // Fetch height
      memcpy(&size_buf_h, &payload.pData_[2], 3);
      size_buf_h[0] = ((size_buf_h[0] >> 6) & 0x3) | ((size_buf_h[1] & 0x3F) << 0x2);
      size_buf_h[1] = ((size_buf_h[1] >> 6) & 0x3) | ((size_buf_h[2] & 0xF) << 0x2);
      pixelHeight_ = Exiv2::getUShort(size_buf_h, littleEndian) + 1;
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_ANMF) && !has_canvas_data) {
      enforce(size >= 12, Exiv2::kerCorruptedMetadata);

      has_canvas_data = true;
      byte size_buf[WEBP_TAG_SIZE];

      readOrThrow(*io_, payload.pData_, payload.size_, Exiv2::kerCorruptedMetadata);

      // Fetch width
      memcpy(&size_buf, &payload.pData_[6], 3);
      size_buf[3] = 0;
      pixelWidth_ = Exiv2::getULong(size_buf, littleEndian) + 1;

      // Fetch height
      memcpy(&size_buf, &payload.pData_[9], 3);
      size_buf[3] = 0;
      pixelHeight_ = Exiv2::getULong(size_buf, littleEndian) + 1;
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_ICCP)) {
      readOrThrow(*io_, payload.pData_, payload.size_, Exiv2::kerCorruptedMetadata);
      // this->setIccProfile(payload);
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_EXIF)) {
      readOrThrow(*io_, payload.pData_, payload.size_, Exiv2::kerCorruptedMetadata);

      byte size_buff2[2];
      // 4 meaningful bytes + 2 padding bytes
      byte exifLongHeader[] = {0xFF, 0x01, 0xFF, 0xE1, 0x00, 0x00};
      byte exifShortHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
      byte exifTiffLEHeader[] = {0x49, 0x49, 0x2A};        // "MM*"
      byte exifTiffBEHeader[] = {0x4D, 0x4D, 0x00, 0x2A};  // "II\0*"
      long offset = 0;
      bool s_header = false;
      bool le_header = false;
      bool be_header = false;
      long pos = getHeaderOffset(payload.pData_, payload.size_, reinterpret_cast<byte*>(&exifLongHeader), 4);

      if (pos == -1) {
        pos = getHeaderOffset(payload.pData_, payload.size_, reinterpret_cast<byte*>(&exifLongHeader), 6);
        if (pos != -1) {
          s_header = true;
        }
      }
      if (pos == -1) {
        pos = getHeaderOffset(payload.pData_, payload.size_, reinterpret_cast<byte*>(&exifTiffLEHeader), 3);
        if (pos != -1) {
          le_header = true;
        }
      }
      if (pos == -1) {
        pos = getHeaderOffset(payload.pData_, payload.size_, reinterpret_cast<byte*>(&exifTiffBEHeader), 4);
        if (pos != -1) {
          be_header = true;
        }
      }

      if (s_header) {
        offset += 6;
      }
      if (be_header || le_header) {
        offset += 12;
      }

      const long sizePayload = payload.size_ + offset;
      byte* rawExifData = new byte[sizePayload];

      if (s_header) {
        us2Data(size_buff2, static_cast<uint16_t>(sizePayload - 6), bigEndian);
        memcpy(rawExifData, reinterpret_cast<char*>(&exifLongHeader), 4);
        memcpy(rawExifData + 4, reinterpret_cast<char*>(&size_buff2), 2);
      }

      if (be_header || le_header) {
        us2Data(size_buff2, static_cast<uint16_t>(sizePayload - 6), bigEndian);
        memcpy(rawExifData, reinterpret_cast<char*>(&exifLongHeader), 4);
        memcpy(rawExifData + 4, reinterpret_cast<char*>(&size_buff2), 2);
        memcpy(rawExifData + 6, reinterpret_cast<char*>(&exifShortHeader), 6);
      }

      memcpy(rawExifData + offset, payload.pData_, payload.size_);

      if (pos != -1) {
        ByteOrder bo = ExifParser::decode(exifData_, payload.pData_ + pos, payload.size_ - pos);
        setByteOrder(bo);
      } else {
        EXV_WARNING << "Failed to decode Exif metadata." << std::endl;
      }

      delete[] rawExifData;
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_XMP)) {
      readOrThrow(*io_, payload.pData_, payload.size_, Exiv2::kerCorruptedMetadata);
      xmpPacket_.assign(reinterpret_cast<char*>(payload.pData_), payload.size_);
      if (!xmpPacket_.empty() && XmpParser::decode(xmpData_, xmpPacket_)) {
        EXV_WARNING << "Failed to decode XMP metadata." << std::endl;
      }
    } else {
      io_->seek(size, BasicIo::cur);
    }

    if (io_->tell() % 2)
      io_->seek(+1, BasicIo::cur);
  }
}

Image::UniquePtr newWebPInstance(BasicIo::UniquePtr io, bool /*create*/) {
  Image::UniquePtr image(new WebPImage(std::move(io)));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

/* =========================================== */

bool isWebPType(BasicIo& iIo, bool /*advance*/) {
  if (iIo.size() < 12) {
    return false;
  }
  const int32_t len = 4;
  const unsigned char RiffImageId[4] = {'R', 'I', 'F', 'F'};
  const unsigned char WebPImageId[4] = {'W', 'E', 'B', 'P'};
  byte webp[len];
  byte data[len];
  byte riff[len];
  readOrThrow(iIo, riff, len, Exiv2::kerCorruptedMetadata);
  readOrThrow(iIo, data, len, Exiv2::kerCorruptedMetadata);
  readOrThrow(iIo, webp, len, Exiv2::kerCorruptedMetadata);
  bool matched_riff = (memcmp(riff, RiffImageId, len) == 0);
  bool matched_webp = (memcmp(webp, WebPImageId, len) == 0);
  iIo.seek(-12, BasicIo::cur);
  return matched_riff && matched_webp;
}  // Exiv2::isWebPType

/*!
     @brief Function used to check equality of a Tags with a
     particular string (ignores case while comparing).
     @param buf Data buffer that will contain Tag to compare
     @param str char* Pointer to string
     @return Returns true if the buffer value is equal to string.
     */
bool WebPImage::equalsWebPTag(Exiv2::DataBuf& buf, const char* str) {
  for (int i = 0; i < 4; i++)
    if (toupper(buf.pData_[i]) != str[i])
      return false;
  return true;
}

/*!
     @brief Function used to add missing EXIF & XMP flags
     to the feature section.
     @param  iIo get BasicIo pointer to inject data
     @param has_xmp Verify if we have xmp data and set required flag
     @param has_exif Verify if we have exif data and set required flag
     @return Returns void
     */
void WebPImage::inject_VP8X(BasicIo& iIo, bool has_xmp, bool has_exif, bool has_alpha, bool has_icc, int width,
                            int height) {
  byte size[4] = {0x0A, 0x00, 0x00, 0x00};
  byte data[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  iIo.write(reinterpret_cast<const byte*>(WEBP_CHUNK_HEADER_VP8X), WEBP_TAG_SIZE);
  iIo.write(size, WEBP_TAG_SIZE);

  if (has_alpha) {
    data[0] |= WEBP_VP8X_ALPHA_BIT;
  }

  if (has_icc) {
    data[0] |= WEBP_VP8X_ICC_BIT;
  }

  if (has_xmp) {
    data[0] |= WEBP_VP8X_XMP_BIT;
  }

  if (has_exif) {
    data[0] |= WEBP_VP8X_EXIF_BIT;
  }

  /* set width - stored in 24bits*/
  int w = width - 1;
  data[4] = w & 0xFF;
  data[5] = (w >> 8) & 0xFF;
  data[6] = (w >> 16) & 0xFF;

  /* set height - stored in 24bits */
  int h = height - 1;
  data[7] = h & 0xFF;
  data[8] = (h >> 8) & 0xFF;
  data[9] = (h >> 16) & 0xFF;

  iIo.write(data, 10);

  /* Handle inject an icc profile right after VP8X chunk */
  if (has_icc) {
    byte size_buff[WEBP_TAG_SIZE];
    ul2Data(size_buff, iccProfile_.size_, littleEndian);
    if (iIo.write(reinterpret_cast<const byte*>(WEBP_CHUNK_HEADER_VP8X), WEBP_TAG_SIZE) != WEBP_TAG_SIZE)
      throw Error(kerImageWriteFailed);
    if (iIo.write(size_buff, WEBP_TAG_SIZE) != WEBP_TAG_SIZE)
      throw Error(kerImageWriteFailed);
    if (iIo.write(iccProfile_.pData_, iccProfile_.size_) != iccProfile_.size_)
      throw Error(kerImageWriteFailed);
    if (iIo.tell() % 2) {
      if (iIo.write(&WEBP_PAD_ODD, 1) != 1)
        throw Error(kerImageWriteFailed);
    }

    has_icc = false;
  }
}

long WebPImage::getHeaderOffset(byte* data, long data_size, byte* header, long header_size) {
  if (data_size < header_size) {
    return -1;
  }
  long pos = -1;
  for (long i = 0; i < data_size - header_size; i++) {
    if (memcmp(header, &data[i], header_size) == 0) {
      pos = i;
      break;
    }
  }
  return pos;
}

}  // namespace Exiv2
