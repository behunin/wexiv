// SPDX-License-Identifier: GPL-2.0-or-later
/*
  File:      crwimage.cpp
  Author(s): Andreas Huggel (ahu) <ahuggel@gmx.net>
  History:   28-Aug-05, ahu: created
 */
// included header files
#include "crwimage.hpp"

#include "config.h"
#include "crwimage_int.hpp"
#include "error.hpp"
#include "tags.hpp"
#include "tags_int.hpp"

// *****************************************************************************
namespace Exiv2 {

using namespace Internal;

CrwImage::CrwImage(BasicIo::UniquePtr io, bool /*create*/) : Image(ImageType::crw, mdExif | mdComment, std::move(io)) {
}  // CrwImage::CrwImage

std::string CrwImage::mimeType() const {
  return "image/x-canon-crw";
}

int CrwImage::pixelWidth() const {
  if (exifData_.hasOwnProperty(photo_pix_X)) {
    return exifData_[photo_pix_X].as<int>();
  }
  return 0;
}

int CrwImage::pixelHeight() const {
  if (exifData_.hasOwnProperty(photo_pix_Y)) {
    return exifData_[photo_pix_Y].as<int>();
  }
  return 0;
}

void CrwImage::readMetadata() {
  if (io_->open() != 0) {
    throw Error(ErrorCode::kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isCrwType(*io_, false)) {
    if (io_->error() || io_->eof())
      throw Error(ErrorCode::kerFailedToReadImageData);
    throw Error(ErrorCode::kerNotACrwImage);
  }

  DataBuf file(static_cast<long>(io().size()));
  io_->read(file.data(), file.size());

  CrwParser::decode(this, io_->mmap(), static_cast<uint32_t>(io_->size()));

}  // CrwImage::readMetadata

void CrwParser::decode(CrwImage* pCrwImage, const byte* pData, uint32_t size) {
  assert(pCrwImage != 0);
  assert(pData != 0);

  // Parse the image, starting with a CIFF header component
  CiffHeader::UniquePtr head(new CiffHeader);
  head->read(pData, size);
  head->decode(*pCrwImage);

  // a hack to get absolute offset of preview image inside CRW structure
  CiffComponent* preview = head->findComponent(0x2007, 0x0000);
  if (preview) {
    uint32_t tmp(preview->pData() - pData);
    pCrwImage->exifData().set("Exif.Image2.JPEGInterchangeFormat", tmp);
    pCrwImage->exifData().set("Exif.Image2.JPEGInterchangeFormatLength", preview->size());
  }
}  // CrwParser::decode

Image::UniquePtr newCrwInstance(BasicIo::UniquePtr io, bool create) {
  auto image = std::make_unique<CrwImage>(std::move(io), create);
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isCrwType(BasicIo& iIo, bool advance) {
  bool result = true;
  byte tmpBuf[14];
  iIo.read(tmpBuf, 14);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  if (!(('I' == tmpBuf[0] && 'I' == tmpBuf[1]) || ('M' == tmpBuf[0] && 'M' == tmpBuf[1]))) {
    result = false;
  }
  if (result && std::memcmp(tmpBuf + 6, CiffHeader::signature(), 8) != 0) {
    result = false;
  }
  if (!advance || !result)
    iIo.seek(-14, BasicIo::cur);
  return result;
}

}  // namespace Exiv2
