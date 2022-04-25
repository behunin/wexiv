// SPDX-License-Identifier: GPL-2.0-or-later

// included header files
#include "rafimage.hpp"

#include "basicio.hpp"
#include "config.h"
#include "enforce.hpp"
#include "error.hpp"
#include "image.hpp"
#include "image_int.hpp"
#include "safe_op.hpp"
#include "tiffimage.hpp"

namespace Exiv2 {

RafImage::RafImage(BasicIo::UniquePtr io, bool /*create*/) :
    Image(ImageType::raf, mdExif | mdIptc | mdXmp, std::move(io)) {
}

std::string RafImage::mimeType() const {
  return "image/x-fuji-raf";
}

int RafImage::pixelWidth() const {
  if (exifData_.hasOwnProperty(photo_pix_X)) {
    return exifData_[photo_pix_X].as<int>();
  }
  return 0;
}

int RafImage::pixelHeight() const {
  if (exifData_.hasOwnProperty(photo_pix_Y)) {
    return exifData_[photo_pix_Y].as<int>();
  }
  return 0;
}

void RafImage::readMetadata() {
  if (io_->open() != 0)
    throw Error(ErrorCode::kerDataSourceOpenFailed, io_->path());
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isRafType(*io_, false)) {
    if (io_->error() || io_->eof())
      throw Error(ErrorCode::kerFailedToReadImageData);
    throw Error(ErrorCode::kerNotAnImage, "RAF");
  }

  if (io_->seek(84, BasicIo::beg) != 0)
    throw Error(kerFailedToReadImageData);
  byte jpg_img_offset[4];
  if (io_->read(jpg_img_offset, 4) != 4)
    throw Error(kerFailedToReadImageData);
  byte jpg_img_length[4];
  if (io_->read(jpg_img_length, 4) != 4)
    throw Error(kerFailedToReadImageData);
  uint32_t jpg_img_off_u32 = Exiv2::getULong(jpg_img_offset, bigEndian);
  uint32_t jpg_img_len_u32 = Exiv2::getULong(jpg_img_length, bigEndian);

  enforce(Safe::add(jpg_img_off_u32, jpg_img_len_u32) <= io_->size(), ErrorCode::kerCorruptedMetadata);

#if LONG_MAX < UINT_MAX
  enforce(jpg_img_off_u32 <= static_cast<uint32_t>(std::numeric_limits<long>::max()), kerCorruptedMetadata);
  enforce(jpg_img_len_u32 <= static_cast<uint32_t>(std::numeric_limits<long>::max()), kerCorruptedMetadata);
#endif

  long jpg_img_off = static_cast<long>(jpg_img_off_u32);
  long jpg_img_len = static_cast<long>(jpg_img_len_u32);

  enforce(jpg_img_len >= 12, ErrorCode::kerCorruptedMetadata);

  DataBuf buf(jpg_img_len - 12);
  if (io_->seek(jpg_img_off + 12, BasicIo::beg) != 0)
    throw Error(ErrorCode::kerFailedToReadImageData);
  io_->read(buf.data(), buf.size());
  if (io_->error() || io_->eof())
    throw Error(ErrorCode::kerFailedToReadImageData);

  io_->seek(0, BasicIo::beg);  // rewind

  ByteOrder bo = TiffParser::decode(exifData_, iptcData_, xmpData_, buf.c_data(), buf.size());

  exifData_.set("Exif.Image2.JPEGInterchangeFormat", getULong(jpg_img_offset, bigEndian));
  exifData_.set("Exif.Image2.JPEGInterchangeFormatLength", getULong(jpg_img_length, bigEndian));

  setByteOrder(bo);

  // parse the tiff
  byte readBuff[4];
  if (io_->seek(100, BasicIo::beg) != 0)
    throw Error(kerFailedToReadImageData);
  if (io_->read(readBuff, 4) != 4)
    throw Error(kerFailedToReadImageData);
  uint32_t tiffOffset = Exiv2::getULong(readBuff, bigEndian);

  if (io_->read(readBuff, 4) != 4)
    throw Error(kerFailedToReadImageData);
  uint32_t tiffLength = Exiv2::getULong(readBuff, bigEndian);

  // sanity check.  Does tiff lie inside the file?
  enforce(Safe::add(tiffOffset, tiffLength) <= io_->size(), kerCorruptedMetadata);

  DataBuf tiff(tiffLength);
  if (io_->seek(tiffOffset, BasicIo::beg) != 0)
    throw Error(kerFailedToReadImageData);
  io_->read(tiff.data(), tiff.size());

  if (!io_->error() && !io_->eof()) {
    TiffParser::decode(exifData_, iptcData_, xmpData_, tiff.c_data(), tiff.size());
  }
}  // RafImage::readMetadata

Image::UniquePtr newRafInstance(BasicIo::UniquePtr io, bool create) {
  auto image = std::make_unique<RafImage>(std::move(io), create);
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isRafType(BasicIo& iIo, bool advance) {
  const int32_t len = 8;
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  int rc = memcmp(buf, "FUJIFILM", 8);
  if (!advance || rc != 0) {
    iIo.seek(-len, BasicIo::cur);
  }
  return rc == 0;
}  // Exiv2::isRafType

}  // namespace Exiv2
