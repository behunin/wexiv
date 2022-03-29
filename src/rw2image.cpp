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
#include "rw2image.hpp"

#include "config.h"
#include "error.hpp"
#include "image.hpp"
#include "rw2image_int.hpp"
#include "tiffcomposite_int.hpp"
#include "tiffimage_int.hpp"

namespace Exiv2 {

  constexpr const char* pRaw_width = "Exif.PanasonicRaw.SensorWidth";
  constexpr const char* pRaw_height = "Exif.PanasonicRaw.SensorHeight";

using namespace Internal;

Rw2Image::Rw2Image(BasicIo::UniquePtr io) : Image(ImageType::rw2, mdExif | mdIptc | mdXmp, std::move(io)) {
}

std::string Rw2Image::mimeType() const {
  return "image/x-panasonic-rw2";
}

int Rw2Image::pixelWidth() const {
  if (exifData_.hasOwnProperty(pRaw_width)) {
    return exifData_[pRaw_width].as<int>();
  }
  return 0;
}

int Rw2Image::pixelHeight() const {
  if (exifData_.hasOwnProperty(pRaw_height)) {
    return exifData_[pRaw_height].as<int>();
  }
  return 0;
}

void Rw2Image::readMetadata() {
  if (io_->open() != 0) {
    throw Error(kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isRw2Type(*io_, false)) {
    if (io_->error() || io_->eof())
      throw Error(kerFailedToReadImageData);
    throw Error(kerNotAnImage, "RW2");
  }

  ByteOrder bo = Rw2Parser::decode(exifData_, iptcData_, xmpData_, io_->mmap(), static_cast<uint32_t>(io_->size()));
  setByteOrder(bo);

  // A lot more metadata is hidden in the embedded preview image
  // Todo: This should go into the Rw2Parser, but for that it needs the Image

} // Rw2Image::readMetadata

ByteOrder Rw2Parser::decode(emscripten::val& exifData, emscripten::val& iptcData, emscripten::val& xmpData, const byte* pData, uint32_t size) {
  Rw2Header rw2Header;
  return TiffParserWorker::decode(exifData, iptcData, xmpData, pData, size, Tag::pana, TiffMapping::findDecoder, &rw2Header);
}

Image::UniquePtr newRw2Instance(BasicIo::UniquePtr io, bool /*create*/) {
  Image::UniquePtr image(new Rw2Image(std::move(io)));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isRw2Type(BasicIo& iIo, bool advance) {
  const int32_t len = 24;
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  Rw2Header header;
  bool rc = header.read(buf, len);
  if (!advance || !rc) {
    iIo.seek(-len, BasicIo::cur);
  }
  return rc;
} // Exiv2::isRw2Type

} // namespace Exiv2
