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
#include "tiffimage.hpp"

#include "basicio.hpp"
#include "config.h"
#include "error.hpp"
#include "i18n.h" // NLS support.
#include "image.hpp"
#include "image_int.hpp"
#include "makernote_int.hpp"
#include "nikonmn_int.hpp"
#include "orfimage.hpp"
#include "tiffcomposite_int.hpp"
#include "tiffimage_int.hpp"
#include "tiffvisitor_int.hpp"
#include "types.hpp"

/* --------------------------------------------------------------------------

   Todo:

   + CR2 Makernotes don't seem to have a next pointer but Canon Jpeg Makernotes
     do. What a mess. (That'll become an issue when it comes to writing to CR2)
   + Sony makernotes in RAW files do not seem to have header like those in Jpegs.
     And maybe no next pointer either.

   in crwimage.* :

   + Fix CiffHeader according to TiffHeader
   + Combine Error(kerNotAJpeg) and Error(kerNotACrwImage), add format argument %1
   + Search crwimage for todos, fix writeMetadata comment
   + rename loadStack to getPath for consistency

   -------------------------------------------------------------------------- */

// *****************************************************************************
// class member definitions
namespace Exiv2 {

using namespace Internal;

TiffImage::TiffImage(BasicIo::UniquePtr io, bool /*create*/)
  : Image(ImageType::tiff, mdExif | mdIptc | mdXmp, std::move(io)), pixelWidthPrimary_(0), pixelHeightPrimary_(0) {
} // TiffImage::TiffImage

//! Structure for TIFF compression to MIME type mappings
struct MimeTypeList {
  //! Comparison operator for compression
  bool operator==(int compression) const { return compression_ == compression; }
  int compression_; //!< TIFF compression
  const char* mimeType_; //!< MIME type
};

//! List of TIFF compression to MIME type mappings
constexpr MimeTypeList mimeTypeList[] = {
    {32770, "image/x-samsung-srw"},
    {34713, "image/x-nikon-nef"},
    {65535, "image/x-pentax-pef"},
};

std::string TiffImage::mimeType() const {
  if (!mimeType_.empty())
    return mimeType_;

  mimeType_ = std::string("image/tiff");
  std::string key = "Exif." + primaryGroup() + ".Compression";
  if (exifData_.hasOwnProperty(key.c_str())) {
    const MimeTypeList* i = find(mimeTypeList, exifData_[key.c_str()].as<int>());
    if (i)
      mimeType_ = std::string(i->mimeType_);
  }
  return mimeType_;
}

std::string TiffImage::primaryGroup() const {
  if (!primaryGroup_.empty())
    return primaryGroup_;

  static const char* keys[] = {"Exif.Image.NewSubfileType",     "Exif.SubImage1.NewSubfileType", "Exif.SubImage2.NewSubfileType",
                               "Exif.SubImage3.NewSubfileType", "Exif.SubImage4.NewSubfileType", "Exif.SubImage5.NewSubfileType",
                               "Exif.SubImage6.NewSubfileType", "Exif.SubImage7.NewSubfileType", "Exif.SubImage8.NewSubfileType",
                               "Exif.SubImage9.NewSubfileType"};
  // Find the group of the primary image, default to "Image"
  primaryGroup_ = std::string("Image");
  for (auto i : keys) {
    // Is it the primary image?
    if (exifData_.hasOwnProperty(i)) {
      // Sometimes there is a JPEG primary image; that's not our first choice
      primaryGroup_ = ExifKey(i).groupName();
      std::string key = "Exif." + primaryGroup_ + ".JPEGInterchangeFormat";
      if (exifData_.hasOwnProperty(key.c_str()))
        break;
    }
  }
  return primaryGroup_;
}

int TiffImage::pixelWidth() const {
  if (pixelWidthPrimary_ != 0) {
    return pixelWidthPrimary_;
  }

  std::string key("Exif." + primaryGroup() + ".ImageWidth");
  if (exifData_.hasOwnProperty(key.c_str())) {
    pixelWidthPrimary_ = exifData_[key.c_str()].as<int>();
  }
  return pixelWidthPrimary_;
}

int TiffImage::pixelHeight() const {
  if (pixelHeightPrimary_ != 0) {
    return pixelHeightPrimary_;
  }

  std::string key("Exif." + primaryGroup() + ".ImageLength");
  if (exifData_.hasOwnProperty(key.c_str())) {
    pixelHeightPrimary_ = exifData_[key.c_str()].as<int>();
  }
  return pixelHeightPrimary_;
}

void TiffImage::readMetadata() {
  if (io_->open() != 0) {
    throw Error(kerDataSourceOpenFailed, io_->path());
  }

  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isTiffType(*io_, false)) {
    if (io_->error() || io_->eof())
      throw Error(kerFailedToReadImageData);
    throw Error(kerNotAnImage, "TIFF");
  }


  ByteOrder bo = TiffParser::decode(exifData_, iptcData_, xmpData_, io_->mmap(), static_cast<uint32_t>(io_->size()));
  setByteOrder(bo);

  // read profile from the metadata
  const char* key("Exif.Image.InterColorProfile");
  if (exifData_.hasOwnProperty(key)) {
    long size = exifData_[key].as<long>();
    if (size == 0) {
      throw Error(kerFailedToReadImageData);
    }
    iccProfile_.alloc(size);
    std::memcpy(iccProfile_.pData_, &bo, sizeof(bo));
  }
}

ByteOrder TiffParser::decode(emscripten::val& exifData, emscripten::val& iptcData, emscripten::val& xmpData, const byte* pData, uint32_t size) {
  uint32_t root = Tag::root;

  // #1402  Fujifilm RAF. Change root when parsing embedded tiff
  const char* key("Exif.Image.Make");
  if (exifData.hasOwnProperty(key)) {
    if (exifData[key].as<std::string>() == "FUJIFILM") {
      root = Tag::fuji;
    }
  }

  return TiffParserWorker::decode(exifData, iptcData, xmpData, pData, size, root, TiffMapping::findDecoder);
} // TiffParser::decode

Image::UniquePtr newTiffInstance(BasicIo::UniquePtr io, bool create) {
  Image::UniquePtr image(new TiffImage(std::move(io), create));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isTiffType(BasicIo& iIo, bool advance) {
  const int32_t len = 8;
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  TiffHeader tiffHeader;
  bool rc = tiffHeader.read(buf, len);
  if (!advance || !rc) {
    iIo.seek(-len, BasicIo::cur);
  }
  return rc;
} // Exiv2::isTiffType

} // namespace Exiv2
