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
#include "mrwimage.hpp"

#include "basicio.hpp"
#include "config.h"
#include "enforce.hpp"
#include "error.hpp"
#include "image.hpp"
#include "tiffimage.hpp"

// *****************************************************************************
namespace Exiv2 {

MrwImage::MrwImage(BasicIo::UniquePtr io, bool /*create*/) :
    Image(ImageType::mrw, mdExif | mdIptc | mdXmp, std::move(io)) {
}  // MrwImage::MrwImage

std::string MrwImage::mimeType() const {
  return "image/x-minolta-mrw";
}

int MrwImage::pixelWidth() const {
  if (exifData_.hasOwnProperty(photo_pix_X)) {
    return exifData_[photo_pix_X].as<int>();
  }
  return 0;
}

int MrwImage::pixelHeight() const {
  if (exifData_.hasOwnProperty(photo_pix_Y)) {
    return exifData_[photo_pix_Y].as<int>();
  }
  return 0;
}

void MrwImage::readMetadata() {
  if (io_->open() != 0) {
    throw Error(kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isMrwType(*io_, false)) {
    if (io_->error() || io_->eof())
      throw Error(kerFailedToReadImageData);
    throw Error(kerNotAnImage, "MRW");
  }

  // Find the TTW block and read it into a buffer
  uint32_t const len = 8;
  byte tmp[len];
  io_->read(tmp, len);
  uint32_t pos = len;
  uint32_t const end = getULong(tmp + 4, bigEndian);

  pos += len;
  enforce(pos <= end, kerFailedToReadImageData);
  io_->read(tmp, len);
  if (io_->error() || io_->eof())
    throw Error(kerFailedToReadImageData);

  while (memcmp(tmp + 1, "TTW", 3) != 0) {
    uint32_t const siz = getULong(tmp + 4, bigEndian);
    enforce(siz <= end - pos, kerFailedToReadImageData);
    pos += siz;
    io_->seek(siz, BasicIo::cur);
    enforce(!io_->error() && !io_->eof(), kerFailedToReadImageData);

    enforce(len <= end - pos, kerFailedToReadImageData);
    pos += len;
    io_->read(tmp, len);
    enforce(!io_->error() && !io_->eof(), kerFailedToReadImageData);
  }

  const uint32_t siz = getULong(tmp + 4, bigEndian);
  // First do an approximate bounds check of siz, so that we don't
  // get DOS-ed by a 4GB allocation on the next line. If siz is
  // greater than io_->size() then it is definitely invalid. But the
  // exact bounds checking is done by the call to io_->read, which
  // will fail if there are fewer than siz bytes left to read.
  enforce(siz <= io_->size(), kerFailedToReadImageData);
  DataBuf buf(siz);
  io_->read(buf.pData_, buf.size_);
  enforce(!io_->error() && !io_->eof(), kerFailedToReadImageData);

  ByteOrder bo = TiffParser::decode(exifData_, iptcData_, xmpData_, buf.pData_, buf.size_);
  setByteOrder(bo);
}  // MrwImage::readMetadata

Image::UniquePtr newMrwInstance(BasicIo::UniquePtr io, bool create) {
  Image::UniquePtr image(new MrwImage(std::move(io), create));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

// *************************************************************************

bool isMrwType(BasicIo& iIo, bool advance) {
  const int32_t len = 4;
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  int rc = memcmp(buf, "\0MRM", 4);
  if (!advance || rc != 0) {
    iIo.seek(-len, BasicIo::cur);
  }
  return rc == 0;
}

}  // namespace Exiv2
