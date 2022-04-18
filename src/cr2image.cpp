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
  File:      cr2image.cpp
 */
// *****************************************************************************
// included header files
#include "cr2image.hpp"

#include "config.h"
#include "cr2header_int.hpp"
#include "error.hpp"
#include "i18n.h"  // NLS support.
#include "image.hpp"
#include "tiffcomposite_int.hpp"
#include "tiffimage.hpp"
#include "tiffimage_int.hpp"

// + standard includes
#include <cassert>

// *****************************************************************************
namespace Exiv2 {

using namespace Internal;

Cr2Image::Cr2Image(BasicIo::UniquePtr io, bool /*create*/) :
    Image(ImageType::cr2, mdExif | mdIptc | mdXmp, std::move(io)) {
}  // Cr2Image::Cr2Image

std::string Cr2Image::mimeType() const {
  return "image/x-canon-cr2";
}

int Cr2Image::pixelWidth() const {
  if (exifData_.hasOwnProperty(photo_pix_X)) {
    return exifData_[photo_pix_X].as<int>();
  }
  return 0;
}

int Cr2Image::pixelHeight() const {
  if (exifData_.hasOwnProperty(photo_pix_Y)) {
    return exifData_[photo_pix_Y].as<int>();
  }
  return 0;
}

void Cr2Image::readMetadata() {
  if (io_->open() != 0) {
    throw Error(kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isCr2Type(*io_, false)) {
    if (io_->error() || io_->eof())
      throw Error(kerFailedToReadImageData);
    throw Error(kerNotAnImage, "CR2");
  }

  ByteOrder bo = Cr2Parser::decode(exifData_, iptcData_, xmpData_, io_->mmap(), static_cast<uint32_t>(io_->size()));
  setByteOrder(bo);
}  // Cr2Image::readMetadata

ByteOrder Cr2Parser::decode(emscripten::val& exifData, emscripten::val& iptcData, emscripten::val& xmpData,
                            const byte* pData, uint32_t size) {
  Cr2Header cr2Header;
  return TiffParserWorker::decode(exifData, iptcData, xmpData, pData, size, Tag::root, TiffMapping::findDecoder,
                                  &cr2Header);
}

Image::UniquePtr newCr2Instance(BasicIo::UniquePtr io, bool create) {
  Image::UniquePtr image(new Cr2Image(std::move(io), create));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isCr2Type(BasicIo& iIo, bool advance) {
  const int32_t len = 16;
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  Cr2Header header;
  bool rc = header.read(buf, len);
  if (!advance || !rc) {
    iIo.seek(-len, BasicIo::cur);
  }
  return rc;
}

}  // namespace Exiv2
