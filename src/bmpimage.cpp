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
  File:      bmpimage.cpp
  Author(s): Marco Piovanelli, Ovolab (marco)
  History:   05-Mar-2007, marco: created
 */
// *****************************************************************************
#include "bmpimage.hpp"

#include "basicio.hpp"
#include "config.h"
#include "error.hpp"

// *****************************************************************************
namespace Exiv2 {
BmpImage::BmpImage(BasicIo::UniquePtr io) : Image(ImageType::bmp, mdNone, std::move(io)) {
}

std::string BmpImage::mimeType() const {
  return "image/x-ms-bmp";
}

void BmpImage::readMetadata() {
  if (io_->open() != 0) {
    throw Error(kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isBmpType(*io_, false)) {
    if (io_->error() || io_->eof())
      throw Error(kerFailedToReadImageData);
    throw Error(kerNotAnImage, "BMP");
  }

  /*
    The Windows bitmap header goes as follows -- all numbers are in little-endian byte order:

    offset  length   name                   description
    ======  =======  =====================  =======
      0      2 bytes  signature              always 'BM'
      2      4 bytes  bitmap size
      6      4 bytes  reserved
    10      4 bytes  bitmap offset
    14      4 bytes  header size
    18      4 bytes  bitmap width
    22      4 bytes  bitmap height
    26      2 bytes  plane count
    28      2 bytes  depth
    30      4 bytes  compression            0 = none; 1 = RLE, 8 bits/pixel; 2 = RLE, 4 bits/pixel; 3 = bitfield; 4 = JPEG; 5 = PNG
    34      4 bytes  image size             size of the raw bitmap data, in bytes
    38      4 bytes  horizontal resolution  (in pixels per meter)
    42      4 bytes  vertical resolution    (in pixels per meter)
    46      4 bytes  color count
    50      4 bytes  important colors       number of "important" colors
  */
  byte buf[54];
  if (io_->read(buf, sizeof(buf)) == sizeof(buf)) {
    pixelWidth_ = getLong(buf + 18, littleEndian);
    pixelHeight_ = getLong(buf + 22, littleEndian);
  }
}

Image::UniquePtr newBmpInstance(BasicIo::UniquePtr io, bool /*create*/) {
  Image::UniquePtr image(new BmpImage(std::move(io)));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isBmpType(BasicIo& iIo, bool advance) {
  const int32_t len = 2;
  const unsigned char BmpImageId[2] = {'B', 'M'};
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  bool matched = (memcmp(buf, BmpImageId, len) == 0);
  if (!advance || !matched) {
    iIo.seek(-len, BasicIo::cur);
  }
  return matched;
}
} // namespace Exiv2