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
#include "orfimage.hpp"

#include "basicio.hpp"
#include "config.h"
#include "error.hpp"
#include "image.hpp"
#include "orfimage_int.hpp"
#include "tiffcomposite_int.hpp"
#include "tiffimage.hpp"
#include "tiffimage_int.hpp"
// *****************************************************************************
namespace Exiv2 {

using namespace Internal;

OrfImage::OrfImage(BasicIo::UniquePtr io, bool create)
  : TiffImage(/*ImageType::orf, mdExif | mdIptc | mdXmp,*/ std::move(io), create) {
  setTypeSupported(ImageType::orf, mdExif | mdIptc | mdXmp);
} // OrfImage::OrfImage

std::string OrfImage::mimeType() const {
  return "image/x-olympus-orf";
}

int OrfImage::pixelWidth() const {
  if (exifData_.hasOwnProperty(image_width)) {
    return exifData_[image_width].as<int>();
  }
  return 0;
}

int OrfImage::pixelHeight() const {
  if (exifData_.hasOwnProperty(image_length)) {
    return exifData_[image_length].as<int>();
  }
  return 0;
}

void OrfImage::readMetadata() {
  if (io_->open() != 0) {
    throw Error(kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isOrfType(*io_, false)) {
    if (io_->error() || io_->eof())
      throw Error(kerFailedToReadImageData);
    throw Error(kerNotAnImage, "ORF");
  }

  ByteOrder bo = OrfParser::decode(exifData_, iptcData_, xmpData_, io_->mmap(), static_cast<uint32_t>(io_->size()));
  setByteOrder(bo);
} // OrfImage::readMetadata

ByteOrder OrfParser::decode(emscripten::val& exifData, emscripten::val& iptcData, emscripten::val& xmpData, const byte* pData, uint32_t size) {
  OrfHeader orfHeader;
  return TiffParserWorker::decode(exifData, iptcData, xmpData, pData, size, Tag::root, TiffMapping::findDecoder, &orfHeader);
}

Image::UniquePtr newOrfInstance(BasicIo::UniquePtr io, bool create) {
  Image::UniquePtr image(new OrfImage(std::move(io), create));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isOrfType(BasicIo& iIo, bool advance) {
  const int32_t len = 8;
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  OrfHeader orfHeader;
  bool rc = orfHeader.read(buf, len);
  if (!advance || !rc) {
    iIo.seek(-len, BasicIo::cur);
  }
  return rc;
}

} // namespace Exiv2
