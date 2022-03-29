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
#include "image.hpp"

#include "bmffimage.hpp"
#include "bmpimage.hpp"
#include "config.h"
#include "cr2image.hpp"
#include "crwimage.hpp"
#include "enforce.hpp"
#include "epsimage.hpp"
#include "error.hpp"
#include "gifimage.hpp"
#include "image_int.hpp"
#include "jp2image.hpp"
#include "jpgimage.hpp"
#include "mrwimage.hpp"
#include "nikonmn_int.hpp"
#include "orfimage.hpp"
#include "pgfimage.hpp"
#include "pngimage.hpp"
#include "psdimage.hpp"
#include "rafimage.hpp"
#include "rw2image.hpp"
#include "safe_op.hpp"
#include "slice.hpp"
#include "tgaimage.hpp"
#include "tiffcomposite_int.hpp"
#include "tiffimage.hpp"
#include "tiffimage_int.hpp"
#include "tiffvisitor_int.hpp"
#include "webpimage.hpp"

// *****************************************************************************
namespace {

using namespace Exiv2;

//! Struct for storing image types and function pointers.
struct Registry {
  //! Comparison operator to compare a Registry structure with an image type
  bool operator==(const int& imageType) const { return imageType == imageType_; }

  // DATA
  int imageType_;
  NewInstanceFct newInstance_;
  IsThisTypeFct isThisType_;
  AccessMode exifSupport_;
  AccessMode iptcSupport_;
  AccessMode xmpSupport_;
  AccessMode commentSupport_;
};

const Registry registry[] = {
    //image type       creation fct     type check  Exif mode    IPTC mode    XMP mode     Comment mode
    //---------------  ---------------  ----------  -----------  -----------  -----------  ------------
    {ImageType::jpeg, newJpegInstance, isJpegType, amReadWrite, amReadWrite, amReadWrite, amReadWrite},
    {ImageType::cr2, newCr2Instance, isCr2Type, amReadWrite, amReadWrite, amReadWrite, amNone},
    {ImageType::crw, newCrwInstance, isCrwType, amReadWrite, amNone, amNone, amReadWrite},
    {ImageType::mrw, newMrwInstance, isMrwType, amRead, amRead, amRead, amNone},
    {ImageType::tiff, newTiffInstance, isTiffType, amReadWrite, amReadWrite, amReadWrite, amNone},
    {ImageType::webp, newWebPInstance, isWebPType, amReadWrite, amNone, amReadWrite, amNone},
    {ImageType::dng, newTiffInstance, isTiffType, amReadWrite, amReadWrite, amReadWrite, amNone},
    {ImageType::nef, newTiffInstance, isTiffType, amReadWrite, amReadWrite, amReadWrite, amNone},
    {ImageType::pef, newTiffInstance, isTiffType, amReadWrite, amReadWrite, amReadWrite, amNone},
    {ImageType::arw, newTiffInstance, isTiffType, amRead, amRead, amRead, amNone},
    {ImageType::rw2, newRw2Instance, isRw2Type, amRead, amRead, amRead, amNone},
    {ImageType::sr2, newTiffInstance, isTiffType, amRead, amRead, amRead, amNone},
    {ImageType::srw, newTiffInstance, isTiffType, amReadWrite, amReadWrite, amReadWrite, amNone},
    {ImageType::orf, newOrfInstance, isOrfType, amReadWrite, amReadWrite, amReadWrite, amNone},
#ifdef EXV_HAVE_LIBZ
    {ImageType::png, newPngInstance, isPngType, amReadWrite, amReadWrite, amReadWrite, amReadWrite},
#endif // EXV_HAVE_LIBZ
    {ImageType::pgf, newPgfInstance, isPgfType, amReadWrite, amReadWrite, amReadWrite, amReadWrite},
    {ImageType::raf, newRafInstance, isRafType, amRead, amRead, amRead, amNone},
    {ImageType::eps, newEpsInstance, isEpsType, amNone, amNone, amReadWrite, amNone},
    {ImageType::gif, newGifInstance, isGifType, amNone, amNone, amNone, amNone},
    {ImageType::psd, newPsdInstance, isPsdType, amReadWrite, amReadWrite, amReadWrite, amNone},
    {ImageType::tga, newTgaInstance, isTgaType, amNone, amNone, amNone, amNone},
    {ImageType::bmp, newBmpInstance, isBmpType, amNone, amNone, amNone, amNone},
    {ImageType::jp2, newJp2Instance, isJp2Type, amReadWrite, amReadWrite, amReadWrite, amNone},
#ifdef EXV_ENABLE_BMFF
    {ImageType::bmff, newBmffInstance, isBmffType, amRead, amRead, amRead, amNone},
#endif // EXV_ENABLE_BMFF \
    // End of list marker
    {ImageType::none, nullptr, nullptr, amNone, amNone, amNone, amNone}};

} // namespace

// *****************************************************************************
namespace Exiv2 {
// BasicIo::read() with error checking
static void readOrThrow(BasicIo& iIo, byte* buf, long rcount, ErrorCode err) {
  const long nread = iIo.read(buf, rcount);
  enforce(nread == rcount, err);
  enforce(!iIo.error(), err);
}

// BasicIo::seek() with error checking
static void seekOrThrow(BasicIo& iIo, long offset, BasicIo::Position pos, ErrorCode err) {
  const int r = iIo.seek(offset, pos);
  enforce(r == 0, err);
}

Image::Image(int imageType, uint16_t supportedMetadata, BasicIo::UniquePtr io)
  : exifData_{emscripten::val::object()},
    iptcData_{emscripten::val::object()},
    xmpData_{emscripten::val::object()},
    headData_{emscripten::val::object()},
    io_(std::move(io)),
    pixelWidth_(0),
    pixelHeight_(0),
    imageType_(imageType),
    supportedMetadata_(supportedMetadata),
#ifdef EXV_HAVE_XMP_TOOLKIT
    writeXmpFromPacket_(false),
#else
    writeXmpFromPacket_(true),
#endif
    byteOrder_(invalidByteOrder),
    init_(true) {
}

bool Image::isStringType(uint16_t type) {
  return type == Exiv2::asciiString || type == Exiv2::unsignedByte || type == Exiv2::signedByte || type == Exiv2::undefined;
}
bool Image::isShortType(uint16_t type) {
  return type == Exiv2::unsignedShort || type == Exiv2::signedShort;
}
bool Image::isLongType(uint16_t type) {
  return type == Exiv2::unsignedLong || type == Exiv2::signedLong;
}
bool Image::isLongLongType(uint16_t type) {
  return type == Exiv2::unsignedLongLong || type == Exiv2::signedLongLong;
}
bool Image::isRationalType(uint16_t type) {
  return type == Exiv2::unsignedRational || type == Exiv2::signedRational;
}
bool Image::is2ByteType(uint16_t type) {
  return isShortType(type);
}
bool Image::is4ByteType(uint16_t type) {
  return isLongType(type) || type == Exiv2::tiffFloat || type == Exiv2::tiffIfd;
}
bool Image::is8ByteType(uint16_t type) {
  return isRationalType(type) || isLongLongType(type) || type == Exiv2::tiffIfd8 || type == Exiv2::tiffDouble;
}
bool Image::isPrintXMP(uint16_t type, Exiv2::PrintStructureOption option) {
  return type == 700 && option == kpsXMP;
}
bool Image::isPrintICC(uint16_t type, Exiv2::PrintStructureOption option) {
  return type == 0x8773 && option == kpsIccProfile;
}

bool Image::isBigEndianPlatform() {
  union {
    uint32_t i;
    char c[4];
  } e = {0x01000000};

  return e.c[0] != 0;
}
bool Image::isLittleEndianPlatform() {
  return !isBigEndianPlatform();
}

uint64_t Image::byteSwap(uint64_t value, bool bSwap) {
  uint64_t result = 0;
  auto source_value = reinterpret_cast<byte*>(&value);
  auto destination_value = reinterpret_cast<byte*>(&result);

  for (int i = 0; i < 8; i++)
    destination_value[i] = source_value[8 - i - 1];

  return bSwap ? result : value;
}

uint32_t Image::byteSwap(uint32_t value, bool bSwap) {
  uint32_t result = 0;
  result |= (value & 0x000000FF) << 24;
  result |= (value & 0x0000FF00) << 8;
  result |= (value & 0x00FF0000) >> 8;
  result |= (value & 0xFF000000) >> 24;
  return bSwap ? result : value;
}

uint16_t Image::byteSwap(uint16_t value, bool bSwap) {
  uint16_t result = 0;
  result |= (value & 0x00FF) << 8;
  result |= (value & 0xFF00) >> 8;
  return bSwap ? result : value;
}

uint16_t Image::byteSwap2(const DataBuf& buf, size_t offset, bool bSwap) {
  uint16_t v = 0;
  auto p = reinterpret_cast<char*>(&v);
  p[0] = buf.pData_[offset];
  p[1] = buf.pData_[offset + 1];
  return Image::byteSwap(v, bSwap);
}

uint32_t Image::byteSwap4(const DataBuf& buf, size_t offset, bool bSwap) {
  uint32_t v = 0;
  auto p = reinterpret_cast<char*>(&v);
  p[0] = buf.pData_[offset];
  p[1] = buf.pData_[offset + 1];
  p[2] = buf.pData_[offset + 2];
  p[3] = buf.pData_[offset + 3];
  return Image::byteSwap(v, bSwap);
}

uint64_t Image::byteSwap8(const DataBuf& buf, size_t offset, bool bSwap) {
  uint64_t v = 0;
  auto p = reinterpret_cast<byte*>(&v);

  for (int i = 0; i < 8; i++)
    p[i] = buf.pData_[offset + i];

  return Image::byteSwap(v, bSwap);
}

const char* Image::typeName(uint16_t tag) {
  //! List of TIFF image tags
  const char* result = nullptr;
  switch (tag) {
    case Exiv2::unsignedByte:
      result = "BYTE";
      break;
    case Exiv2::asciiString:
      result = "ASCII";
      break;
    case Exiv2::unsignedShort:
      result = "SHORT";
      break;
    case Exiv2::unsignedLong:
      result = "LONG";
      break;
    case Exiv2::unsignedRational:
      result = "RATIONAL";
      break;
    case Exiv2::signedByte:
      result = "SBYTE";
      break;
    case Exiv2::undefined:
      result = "UNDEFINED";
      break;
    case Exiv2::signedShort:
      result = "SSHORT";
      break;
    case Exiv2::signedLong:
      result = "SLONG";
      break;
    case Exiv2::signedRational:
      result = "SRATIONAL";
      break;
    case Exiv2::tiffFloat:
      result = "FLOAT";
      break;
    case Exiv2::tiffDouble:
      result = "DOUBLE";
      break;
    case Exiv2::tiffIfd:
      result = "IFD";
      break;
    default:
      result = "unknown";
      break;
  }
  return result;
}

static bool typeValid(uint16_t type) {
  return type >= 1 && type <= 13;
}

emscripten::val& Image::exifData() {
  return exifData_;
}

const emscripten::val& Image::exifData() const {
  return exifData_;
}

void Image::clearExifData() {
  exifData_.object();
}

void Image::setExifData(const emscripten::val& exifData) {
  exifData_ = exifData;
}

emscripten::val& Image::iptcData() {
  return iptcData_;
}

const emscripten::val& Image::iptcData() const {
  return iptcData_;
}

void Image::clearIptcData() {
  iptcData_.object();
}

void Image::setIptcData(const emscripten::val& iptcData) {
  iptcData_ = iptcData;
}

emscripten::val& Image::xmpData() {
  return xmpData_;
}

const emscripten::val& Image::xmpData() const {
  return xmpData_;
}

void Image::clearXmpData() {
  xmpData_.object();
  writeXmpFromPacket(false);
}

void Image::setXmpData(const emscripten::val& xmpData) {
  xmpData_ = xmpData;
  writeXmpFromPacket(false);
}

emscripten::val& Image::headData() {
  return headData_;
}

const emscripten::val& Image::headData() const {
  return headData_;
}

void Image::setHeadData(const emscripten::val& headData) {
  headData_ = headData;
}

void Image::clearHeadData() {
  headData_.object();
}

std::string& Image::xmpPacket() {
  return xmpPacket_;
}

const std::string& Image::xmpPacket() const {
  return xmpPacket_;
}

void Image::clearXmpPacket() {
  xmpPacket_.clear();
  writeXmpFromPacket(true);
}

void Image::setXmpPacket(const std::string& xmpPacket) {
  xmpPacket_ = xmpPacket;
  if (XmpParser::decode(xmpData_, xmpPacket)) {
    throw Error(kerInvalidXMP);
  }
  xmpPacket_ = xmpPacket;
}

std::string Image::comment() const {
  return comment_;
}

void Image::clearComment() {
  comment_.erase();
}

void Image::setComment(const std::string& comment) {
  comment_ = comment;
}

void Image::setIccProfile(Exiv2::DataBuf& iccProfile, bool bTestValid) {
  if (bTestValid) {
    if (iccProfile.pData_ && (iccProfile.size_ < static_cast<long>(sizeof(long))))
      throw Error(kerInvalidIccProfile);
    long size = iccProfile.pData_ ? getULong(iccProfile.pData_, bigEndian) : -1;
    if (size != iccProfile.size_)
      throw Error(kerInvalidIccProfile);
  }
  iccProfile_ = iccProfile;
}

void Image::clearIccProfile() {
  iccProfile_.reset();
}

void Image::writeXmpFromPacket(bool flag) {
  writeXmpFromPacket_ = flag;
}

void Image::setByteOrder(ByteOrder byteOrder) {
  byteOrder_ = byteOrder;
}

ByteOrder Image::byteOrder() const {
  return byteOrder_;
}

int Image::pixelWidth() const {
  return pixelWidth_;
}

int Image::pixelHeight() const {
  return pixelHeight_;
}

BasicIo& Image::io() const {
  return *io_;
}

bool Image::writeXmpFromPacket() const {
  return writeXmpFromPacket_;
}

const NativePreviewList& Image::nativePreviews() const {
  return nativePreviews_;
}

bool Image::good() const {
  if (io_->open() != 0)
    return false;
  IoCloser closer(*io_);
  return ImageFactory::checkType(imageType_, *io_, false);
}

bool Image::supportsMetadata(MetadataId metadataId) const {
  return (supportedMetadata_ & metadataId) != 0;
}

AccessMode Image::checkMode(MetadataId metadataId) const {
  return ImageFactory::checkMode(imageType_, metadataId);
}

const std::string& Image::tagName(uint16_t tag) {
  if (init_) {
    int idx;
    const TagInfo* ti;
    for (ti = Internal::mnTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx)
      tags_[ti[idx].tag_] = ti[idx].name_;
    for (ti = Internal::iopTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx)
      tags_[ti[idx].tag_] = ti[idx].name_;
    for (ti = Internal::gpsTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx)
      tags_[ti[idx].tag_] = ti[idx].name_;
    for (ti = Internal::ifdTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx)
      tags_[ti[idx].tag_] = ti[idx].name_;
    for (ti = Internal::exifTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx)
      tags_[ti[idx].tag_] = ti[idx].name_;
    for (ti = Internal::mpfTagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx)
      tags_[ti[idx].tag_] = ti[idx].name_;
    for (ti = Internal::Nikon1MakerNote::tagList(), idx = 0; ti[idx].tag_ != 0xffff; ++idx)
      tags_[ti[idx].tag_] = ti[idx].name_;
  }
  init_ = false;

  return tags_[tag];
}

AccessMode ImageFactory::checkMode(int type, MetadataId metadataId) {
  const Registry* r = find(registry, type);
  if (!r)
    throw Error(kerUnsupportedImageType, type);
  AccessMode am = amNone;
  switch (metadataId) {
    case mdNone:
      break;
    case mdExif:
      am = r->exifSupport_;
      break;
    case mdIptc:
      am = r->iptcSupport_;
      break;
    case mdXmp:
      am = r->xmpSupport_;
      break;
    case mdComment:
      am = r->commentSupport_;
      break;
    case mdIccProfile:
      break;

      // no default: let the compiler complain
  }
  return am;
}

bool ImageFactory::checkType(int type, BasicIo& io, bool advance) {
  const Registry* r = find(registry, type);
  if (nullptr != r) {
    return r->isThisType_(io, advance);
  }
  return false;
} // ImageFactory::checkType

int ImageFactory::getType(const byte* data, long size) {
  MemIo memIo(data, size);
  return getType(memIo);
}

int ImageFactory::getType(BasicIo& io) {
  if (io.open() != 0)
    return ImageType::none;
  IoCloser closer(io);
  for (unsigned int i = 0; registry[i].imageType_ != ImageType::none; ++i) {
    if (registry[i].isThisType_(io, false)) {
      return registry[i].imageType_;
    }
  }
  return ImageType::none;
} // ImageFactory::getType

Image::UniquePtr ImageFactory::open(const byte* data, long size) {
  BasicIo::UniquePtr io(new MemIo(data, size));
  Image::UniquePtr image = open(std::move(io)); // may throw
  if (image.get() == nullptr)
    throw Error(kerMemoryContainsUnknownImageType);
  return image;
}

Image::UniquePtr ImageFactory::open(BasicIo::UniquePtr io) {
  if (io->open() != 0) {
    throw Error(kerDataSourceOpenFailed, io->path());
  }
  for (unsigned int i = 0; registry[i].imageType_ != ImageType::none; ++i) {
    if (registry[i].isThisType_(*io, false)) {
      return registry[i].newInstance_(std::move(io), false);
    }
  }
  return Image::UniquePtr();
} // ImageFactory::open

// *****************************************************************************

void append(Blob& blob, const byte* buf, uint32_t len) {
  if (len != 0) {
    assert(buf != 0);
    Blob::size_type size = blob.size();
    if (blob.capacity() - size < len) {
      blob.reserve(size + 65536);
    }
    blob.resize(size + len);
    std::memcpy(&blob[size], buf, len);
  }
} // append

} // namespace Exiv2
