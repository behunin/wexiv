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
#include "types.hpp"

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))

// *****************************************************************************
// class member definitions
namespace Exiv2 {
using namespace Exiv2::Internal;

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
    throw Error(ErrorCode::kerDataSourceOpenFailed, io_->path());
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isWebPType(*io_, true)) {
    if (io_->error() || io_->eof())
      throw Error(ErrorCode::kerFailedToReadImageData);
    throw Error(ErrorCode::kerNotAJpeg);
  }

  byte data[12];
  DataBuf chunkId(5);
  chunkId.write_uint8(4, '\0');

  io_->readOrThrow(data, WEBP_TAG_SIZE * 3, Exiv2::ErrorCode::kerCorruptedMetadata);

  const uint32_t filesize_u32 = Safe::add(Exiv2::getULong(data + WEBP_TAG_SIZE, littleEndian), 8U);
  enforce(filesize_u32 <= io_->size(), Exiv2::kerCorruptedMetadata);

  // Check that `filesize_u32` is safe to cast to `long`.
  enforce(filesize_u32 <= static_cast<size_t>(std::numeric_limits<unsigned int>::max()),
          Exiv2::ErrorCode::kerCorruptedMetadata);

  WebPImage::decodeChunks(static_cast<long>(filesize_u32));

}  // WebPImage::readMetadata

void WebPImage::decodeChunks(long filesize) {
  DataBuf chunkId(5);
  byte size_buff[WEBP_TAG_SIZE];
  bool has_canvas_data = false;

  chunkId.write_uint8(4, '\0');
  while (!io_->eof() && io_->tell() < filesize) {
    io_->readOrThrow(chunkId.data(), WEBP_TAG_SIZE, Exiv2::ErrorCode::kerCorruptedMetadata);
    io_->readOrThrow(size_buff, WEBP_TAG_SIZE, Exiv2::ErrorCode::kerCorruptedMetadata);

    const uint32_t size_u32 = Exiv2::getULong(size_buff, littleEndian);

    // Check that `size_u32` is safe to cast to `long`.
    enforce(size_u32 <= static_cast<size_t>(std::numeric_limits<unsigned int>::max()),
            Exiv2::ErrorCode::kerCorruptedMetadata);
    const long size = static_cast<long>(size_u32);

    // Check that `size` is within bounds.
    enforce(io_->tell() <= filesize, Exiv2::kerCorruptedMetadata);
    enforce(size <= (filesize - io_->tell()), Exiv2::kerCorruptedMetadata);

    DataBuf payload(size);

    if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8X) && !has_canvas_data) {
      enforce(size >= 10, Exiv2::ErrorCode::kerCorruptedMetadata);

      has_canvas_data = true;
      byte size_buf[WEBP_TAG_SIZE];

      io_->readOrThrow(payload.data(), payload.size(), Exiv2::ErrorCode::kerCorruptedMetadata);

      // Fetch width
      memcpy(&size_buf, payload.c_data(4), 3);
      size_buf[3] = 0;
      pixelWidth_ = Exiv2::getULong(size_buf, littleEndian) + 1;

      // Fetch height
      memcpy(&size_buf, payload.c_data(7), 3);
      size_buf[3] = 0;
      pixelHeight_ = Exiv2::getULong(size_buf, littleEndian) + 1;
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8) && !has_canvas_data) {
      enforce(size >= 10, Exiv2::ErrorCode::kerCorruptedMetadata);

      has_canvas_data = true;
      io_->readOrThrow(payload.data(), payload.size(), Exiv2::ErrorCode::kerCorruptedMetadata);
      byte size_buf[WEBP_TAG_SIZE];

      // Fetch width""
      memcpy(&size_buf, payload.c_data(6), 2);
      size_buf[2] = 0;
      size_buf[3] = 0;
      pixelWidth_ = Exiv2::getULong(size_buf, littleEndian) & 0x3fff;

      // Fetch height
      memcpy(&size_buf, payload.c_data(8), 2);
      size_buf[2] = 0;
      size_buf[3] = 0;
      pixelHeight_ = Exiv2::getULong(size_buf, littleEndian) & 0x3fff;
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_VP8L) && !has_canvas_data) {
      enforce(size >= 5, Exiv2::ErrorCode::kerCorruptedMetadata);

      has_canvas_data = true;
      byte size_buf_w[2];
      byte size_buf_h[3];

      io_->readOrThrow(payload.data(), payload.size(), Exiv2::ErrorCode::kerCorruptedMetadata);

      // Fetch width
      memcpy(&size_buf_w, payload.c_data(1), 2);
      size_buf_w[1] &= 0x3F;
      pixelWidth_ = Exiv2::getUShort(size_buf_w, littleEndian) + 1;

      // Fetch height
      memcpy(&size_buf_h, payload.c_data(2), 3);
      size_buf_h[0] = ((size_buf_h[0] >> 6) & 0x3) | ((size_buf_h[1] & 0x3F) << 0x2);
      size_buf_h[1] = ((size_buf_h[1] >> 6) & 0x3) | ((size_buf_h[2] & 0xF) << 0x2);
      pixelHeight_ = Exiv2::getUShort(size_buf_h, littleEndian) + 1;
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_ANMF) && !has_canvas_data) {
      enforce(size >= 12, Exiv2::ErrorCode::kerCorruptedMetadata);

      has_canvas_data = true;
      byte size_buf[WEBP_TAG_SIZE];

      io_->readOrThrow(payload.data(), payload.size(), Exiv2::ErrorCode::kerCorruptedMetadata);

      // Fetch width
      memcpy(&size_buf, payload.c_data(6), 3);
      size_buf[3] = 0;
      pixelWidth_ = Exiv2::getULong(size_buf, littleEndian) + 1;

      // Fetch height
      memcpy(&size_buf, payload.c_data(9), 3);
      size_buf[3] = 0;
      pixelHeight_ = Exiv2::getULong(size_buf, littleEndian) + 1;
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_ICCP)) {
      io_->readOrThrow(payload.data(), payload.size(), Exiv2::ErrorCode::kerCorruptedMetadata);
      // this->setIccProfile(payload);
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_EXIF)) {
      io_->readOrThrow(payload.data(), payload.size(), Exiv2::ErrorCode::kerCorruptedMetadata);

      byte size_buff2[2];
      // 4 meaningful bytes + 2 padding bytes
      byte exifLongHeader[] = {0xFF, 0x01, 0xFF, 0xE1, 0x00, 0x00};
      byte exifShortHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
      byte exifTiffLEHeader[] = {0x49, 0x49, 0x2A};        // "MM*"
      byte exifTiffBEHeader[] = {0x4D, 0x4D, 0x00, 0x2A};  // "II\0*"
      size_t offset = 0;
      bool s_header = false;
      bool le_header = false;
      bool be_header = false;
      long pos = getHeaderOffset(payload.c_data(), payload.size(), reinterpret_cast<byte*>(&exifLongHeader), 4);

      if (pos == -1) {
        pos = getHeaderOffset(payload.c_data(), payload.size(), reinterpret_cast<byte*>(&exifLongHeader), 6);
        if (pos != -1) {
          s_header = true;
        }
      }
      if (pos == -1) {
        pos = getHeaderOffset(payload.c_data(), payload.size(), reinterpret_cast<byte*>(&exifTiffLEHeader), 3);
        if (pos != -1) {
          le_header = true;
        }
      }
      if (pos == -1) {
        pos = getHeaderOffset(payload.c_data(), payload.size(), reinterpret_cast<byte*>(&exifTiffBEHeader), 4);
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

      const size_t sizePayload = Safe::add(payload.size(), offset);
      DataBuf rawExifData(sizePayload);

      if (s_header) {
        us2Data(size_buff2, static_cast<uint16_t>(sizePayload - 6), bigEndian);
        std::copy_n(reinterpret_cast<char*>(&exifLongHeader), 4, rawExifData.begin());
        std::copy_n(reinterpret_cast<char*>(&size_buff2), 2, rawExifData.begin() + 4);
      }

      if (be_header || le_header) {
        us2Data(size_buff2, static_cast<uint16_t>(sizePayload - 6), bigEndian);
        std::copy_n(reinterpret_cast<char*>(&exifLongHeader), 4, rawExifData.begin());
        std::copy_n(reinterpret_cast<char*>(&size_buff2), 2, rawExifData.begin() + 4);
        std::copy_n(reinterpret_cast<char*>(&exifShortHeader), 6, rawExifData.begin() + 6);
      }

      std::copy(payload.begin(), payload.end(), rawExifData.begin() + offset);

      if (pos != -1) {
        ByteOrder bo = ExifParser::decode(exifData_, payload.c_data(pos), payload.size() - pos);
        setByteOrder(bo);
      } else {
        EXV_WARNING << "Failed to decode Exif metadata." << std::endl;
      }
    } else if (equalsWebPTag(chunkId, WEBP_CHUNK_HEADER_XMP)) {
      io_->readOrThrow(payload.data(), payload.size(), Exiv2::kerCorruptedMetadata);
      xmpPacket_.assign(payload.c_str(), payload.size());
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
  auto image = std::make_unique<WebPImage>(std::move(io));
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
  iIo.readOrThrow(riff, len, Exiv2::ErrorCode::kerCorruptedMetadata);
  iIo.readOrThrow(data, len, Exiv2::ErrorCode::kerCorruptedMetadata);
  iIo.readOrThrow(webp, len, Exiv2::kerCorruptedMetadata);
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
    if (toupper(buf.read_uint8(i)) != str[i])
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
    ul2Data(size_buff, static_cast<uint32_t>(iccProfile_.size()), littleEndian);
    if (iIo.write(reinterpret_cast<const byte*>(WEBP_CHUNK_HEADER_VP8X), WEBP_TAG_SIZE) != WEBP_TAG_SIZE)
      throw Error(kerImageWriteFailed);
    if (iIo.write(size_buff, WEBP_TAG_SIZE) != WEBP_TAG_SIZE)
      throw Error(kerImageWriteFailed);
    if (iIo.write(iccProfile_.c_data(), iccProfile_.size()) != iccProfile_.size())
      throw Error(kerImageWriteFailed);
    if (iIo.tell() % 2) {
      if (iIo.write(&WEBP_PAD_ODD, 1) != 1)
        throw Error(kerImageWriteFailed);
    }
  }
}

long WebPImage::getHeaderOffset(const byte* data, size_t data_size, const byte* header, size_t header_size) {
  if (data_size < header_size) {
    return -1;
  }
  long pos = -1;
  for (size_t i = 0; i < data_size - header_size; i++) {
    if (memcmp(header, &data[i], header_size) == 0) {
      pos = static_cast<long>(i);
      break;
    }
  }
  return pos;
}

}  // namespace Exiv2
