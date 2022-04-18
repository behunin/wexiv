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
#include "pgfimage.hpp"

#include "basicio.hpp"
#include "config.h"
#include "enforce.hpp"
#include "error.hpp"
#include "image.hpp"
#include "pngimage.hpp"

// Signature from front of PGF file
const unsigned char pgfSignature[3] = {0x50, 0x47, 0x46};

const unsigned char pgfBlank[] = {
    0x50, 0x47, 0x46, 0x36, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x03, 0x03, 0x00, 0x00, 0x00, 0x14, 0x00, 0x67, 0x08, 0x20, 0x00, 0xc0, 0x01, 0x00, 0x00, 0x37, 0x00,
    0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x37, 0x00,
    0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};

// *****************************************************************************
// class member definitions

namespace Exiv2 {

static uint32_t byteSwap_(uint32_t value, bool bSwap) {
  uint32_t result = 0;
  result |= (value & 0x000000FF) << 24;
  result |= (value & 0x0000FF00) << 8;
  result |= (value & 0x00FF0000) >> 8;
  result |= (value & 0xFF000000) >> 24;
  return bSwap ? result : value;
}

static uint32_t byteSwap_(Exiv2::DataBuf& buf, size_t offset, bool bSwap) {
  uint32_t v = 0;
  auto p = reinterpret_cast<char*>(&v);
  int i;
  for (i = 0; i < 4; i++)
    p[i] = buf.pData_[offset + i];
  uint32_t result = byteSwap_(v, bSwap);
  p = reinterpret_cast<char*>(&result);
  for (i = 0; i < 4; i++)
    buf.pData_[offset + i] = p[i];
  return result;
}

PgfImage::PgfImage(BasicIo::UniquePtr io, bool create) :
    Image(ImageType::pgf, mdExif | mdIptc | mdXmp | mdComment, std::move(io)), bSwap_(isBigEndianPlatform()) {
}  // PgfImage::PgfImage

void PgfImage::readMetadata() {
  if (io_->open() != 0) {
    throw Error(kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isPgfType(*io_, true)) {
    if (io_->error() || io_->eof())
      throw Error(kerFailedToReadImageData);
    throw Error(kerNotAnImage, "PGF");
  }

  readPgfMagicNumber(*io_);

  uint32_t headerSize = readPgfHeaderSize(*io_);
  readPgfHeaderStructure(*io_, pixelWidth_, pixelHeight_);

  // And now, the most interresting, the user data byte array where metadata are stored as small image.

  enforce(headerSize <= std::numeric_limits<uint32_t>::max() - 8, kerCorruptedMetadata);
#if LONG_MAX < UINT_MAX
  enforce(headerSize + 8 <= static_cast<uint32_t>(std::numeric_limits<long>::max()), kerCorruptedMetadata);
#endif
  long size = static_cast<long>(headerSize) + 8 - io_->tell();

  if (size < 0 || static_cast<size_t>(size) > io_->size())
    throw Error(kerInputDataReadFailed);
  if (size == 0)
    return;

  DataBuf imgData(size);
  std::memset(imgData.pData_, 0x0, imgData.size_);
  long bufRead = io_->read(imgData.pData_, imgData.size_);
  if (io_->error())
    throw Error(kerFailedToReadImageData);
  if (bufRead != imgData.size_)
    throw Error(kerInputDataReadFailed);

  Image::UniquePtr image = Exiv2::ImageFactory::open(imgData.pData_, imgData.size_);
  image->readMetadata();
  exifData() = image->exifData();
  iptcData() = image->iptcData();
  xmpData() = image->xmpData();

}  // PgfImage::readMetadata

byte PgfImage::readPgfMagicNumber(BasicIo& iIo) {
  byte b = iIo.getb();
  if (iIo.error())
    throw Error(kerFailedToReadImageData);

  if (b < 0x36)  // 0x36 = '6'.
  {
    // Not right Magick version.
    throw Error(kerFailedToReadImageData, "Exiv2::PgfImage::readMetadata: wrong Magick number");
  }

  return b;
}  // PgfImage::readPgfMagicNumber

uint32_t PgfImage::readPgfHeaderSize(BasicIo& iIo) const {
  DataBuf buffer(4);
  long bufRead = iIo.read(buffer.pData_, buffer.size_);
  if (iIo.error())
    throw Error(kerFailedToReadImageData);
  if (bufRead != buffer.size_)
    throw Error(kerInputDataReadFailed);

  int headerSize = static_cast<int>(byteSwap_(buffer, 0, bSwap_));
  if (headerSize <= 0)
    throw Error(kerNoImageInInputData);

  return headerSize;
}  // PgfImage::readPgfHeaderSize

DataBuf PgfImage::readPgfHeaderStructure(BasicIo& iIo, int& width, int& height) const {
  DataBuf header(16);
  long bufRead = iIo.read(header.pData_, header.size_);
  if (iIo.error())
    throw Error(kerFailedToReadImageData);
  if (bufRead != header.size_)
    throw Error(kerInputDataReadFailed);

  DataBuf work(8);  // don't disturb the binary data - doWriteMetadata reuses it
  memcpy(work.pData_, header.pData_, 8);
  width = byteSwap_(work, 0, bSwap_);
  height = byteSwap_(work, 4, bSwap_);

  /* NOTE: properties not yet used
        byte nLevels  = buffer.pData_[8];
        byte quality  = buffer.pData_[9];
        byte bpp      = buffer.pData_[10];
        byte channels = buffer.pData_[11];
        */
  byte mode = header.pData_[12];

  if (mode == 2)  // Indexed color image. We pass color table (256 * 3 bytes).
  {
    header.alloc(16 + 256 * 3);

    bufRead = iIo.read(&header.pData_[16], 256 * 3);
    if (iIo.error())
      throw Error(kerFailedToReadImageData);
    if (bufRead != 256 * 3)
      throw Error(kerInputDataReadFailed);
  }

  return header;
}  // PgfImage::readPgfHeaderStructure

Image::UniquePtr newPgfInstance(BasicIo::UniquePtr io, bool create) {
  Image::UniquePtr image(new PgfImage(std::move(io), create));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isPgfType(BasicIo& iIo, bool advance) {
  const int32_t len = 3;
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  int rc = memcmp(buf, pgfSignature, 3);
  if (!advance || rc != 0) {
    iIo.seek(-len, BasicIo::cur);
  }

  return rc == 0;
}  // Exiv2::isPgfType
}  // namespace Exiv2
