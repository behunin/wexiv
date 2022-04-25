// SPDX-License-Identifier: GPL-2.0-or-later

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
    throw Error(ErrorCode::kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isMrwType(*io_, false)) {
    if (io_->error() || io_->eof())
      throw Error(ErrorCode::kerFailedToReadImageData);
    throw Error(ErrorCode::kerNotAnImage, "MRW");
  }

  // Find the TTW block and read it into a buffer
  uint32_t const len = 8;
  byte tmp[len];
  io_->read(tmp, len);
  uint32_t pos = len;
  uint32_t const end = getULong(tmp + 4, bigEndian);

  pos += len;
  enforce(pos <= end, ErrorCode::kerFailedToReadImageData);
  io_->read(tmp, len);
  if (io_->error() || io_->eof())
    throw Error(ErrorCode::kerFailedToReadImageData);

  while (memcmp(tmp + 1, "TTW", 3) != 0) {
    uint32_t const siz = getULong(tmp + 4, bigEndian);
    enforce(siz <= end - pos, ErrorCode::kerFailedToReadImageData);
    pos += siz;
    io_->seek(siz, BasicIo::cur);
    enforce(!io_->error() && !io_->eof(), ErrorCode::kerFailedToReadImageData);

    enforce(len <= end - pos, ErrorCode::kerFailedToReadImageData);
    pos += len;
    io_->read(tmp, len);
    enforce(!io_->error() && !io_->eof(), ErrorCode::kerFailedToReadImageData);
  }

  const uint32_t siz = getULong(tmp + 4, bigEndian);
  // First do an approximate bounds check of siz, so that we don't
  // get DOS-ed by a 4GB allocation on the next line. If siz is
  // greater than io_->size() then it is definitely invalid. But the
  // exact bounds checking is done by the call to io_->read, which
  // will fail if there are fewer than siz bytes left to read.
  enforce(siz <= io_->size(), ErrorCode::kerFailedToReadImageData);
  DataBuf buf(siz);
  io_->read(buf.data(), buf.size());
  enforce(!io_->error() && !io_->eof(), ErrorCode::kerFailedToReadImageData);

  ByteOrder bo = TiffParser::decode(exifData_, iptcData_, xmpData_, buf.c_data(), buf.size());
  setByteOrder(bo);
}  // MrwImage::readMetadata

Image::UniquePtr newMrwInstance(BasicIo::UniquePtr io, bool create) {
  auto image = std::make_unique<MrwImage>(std::move(io), create);
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
