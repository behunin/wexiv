// SPDX-License-Identifier: GPL-2.0-or-later

// included header files
#include "pgfimage.hpp"

#include "basicio.hpp"
#include "config.h"
#include "enforce.hpp"
#include "error.hpp"
#include "image.hpp"

// Signature from front of PGF file
const unsigned char pgfSignature[3] = {0x50, 0x47, 0x46};

const unsigned char pgfBlank[] = {
    0x50, 0x47, 0x46, 0x36, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x03, 0x03, 0x00, 0x00, 0x00, 0x14, 0x00, 0x67, 0x08, 0x20, 0x00, 0xc0, 0x01, 0x00, 0x00, 0x37, 0x00,
    0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x37, 0x00,
    0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};

// *****************************************************************************
// class member definitions

namespace Exiv2 {

static uint32_t byteSwap_(uint32_t value, bool bSwap) {
  uint32_t result = 0;
  result |= (value & 0x000000FF) << 24;
  result |= (value & 0x0000FF00) << 8;
  result |= (value & 0x00FF0000) >> 8;
  result |= (value & 0xFF000000) >> 24;
  return bSwap ? result : value;
}

static uint32_t byteSwap_(Exiv2::DataBuf& buf, size_t offset, bool bSwap) {
  uint32_t v = 0;
  auto p = reinterpret_cast<byte*>(&v);
  int i;
  for (i = 0; i < 4; i++)
    p[i] = buf.read_uint8(offset + i);
  uint32_t result = byteSwap_(v, bSwap);
  p = reinterpret_cast<byte*>(&result);
  for (i = 0; i < 4; i++)
    buf.write_uint8(offset + i, p[i]);
  return result;
}

PgfImage::PgfImage(BasicIo::UniquePtr io, bool create) :
    Image(ImageType::pgf, mdExif | mdIptc | mdXmp | mdComment, std::move(io)), bSwap_(isBigEndianPlatform()) {
}  // PgfImage::PgfImage

void PgfImage::readMetadata() {
  if (io_->open() != 0) {
    throw Error(ErrorCode::kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isPgfType(*io_, true)) {
    if (io_->error() || io_->eof())
      throw Error(ErrorCode::kerFailedToReadImageData);
    throw Error(ErrorCode::kerNotAnImage, "PGF");
  }

  readPgfMagicNumber(*io_);

  size_t headerSize = readPgfHeaderSize(*io_);
  readPgfHeaderStructure(*io_, pixelWidth_, pixelHeight_);

  // And now, the most interresting, the user data byte array where metadata are stored as small image.

  enforce(headerSize <= std::numeric_limits<size_t>::max() - 8, ErrorCode::kerCorruptedMetadata);
  size_t size = headerSize + 8 - static_cast<size_t>(io_->tell());

  if (size > io_->size())
    throw Error(ErrorCode::kerInputDataReadFailed);
  if (size == 0)
    return;

  DataBuf imgData(size);
  const size_t bufRead = io_->read(imgData.data(), imgData.size());
  if (io_->error())
    throw Error(ErrorCode::kerFailedToReadImageData);
  if (bufRead != imgData.size())
    throw Error(ErrorCode::kerInputDataReadFailed);

  Image::UniquePtr image = Exiv2::ImageFactory::open(imgData.c_data(), imgData.size());
  image->readMetadata();
  exifData() = image->exifData();
  iptcData() = image->iptcData();
  xmpData() = image->xmpData();

}  // PgfImage::readMetadata

byte PgfImage::readPgfMagicNumber(BasicIo& iIo) {
  byte b = iIo.getb();
  if (iIo.error())
    throw Error(kerFailedToReadImageData);

  if (b < 0x36)  // 0x36 = '6'.
  {
    // Not right Magick version.
    throw Error(kerFailedToReadImageData, "Exiv2::PgfImage::readMetadata: wrong Magick number");
  }

  return b;
}  // PgfImage::readPgfMagicNumber

uint32_t PgfImage::readPgfHeaderSize(BasicIo& iIo) const {
  DataBuf buffer(4);
  long bufRead = iIo.read(buffer.data(), buffer.size());
  if (iIo.error())
    throw Error(ErrorCode::kerFailedToReadImageData);
  if (bufRead != buffer.size())
    throw Error(ErrorCode::kerInputDataReadFailed);

  int headerSize = static_cast<int>(byteSwap_(buffer, 0, bSwap_));
  if (headerSize <= 0)
    throw Error(ErrorCode::kerNoImageInInputData);

  return headerSize;
}  // PgfImage::readPgfHeaderSize

DataBuf PgfImage::readPgfHeaderStructure(BasicIo& iIo, uint32_t& width, uint32_t& height) const {
  DataBuf header(16);
  long bufRead = iIo.read(header.data(), header.size());
  if (iIo.error())
    throw Error(ErrorCode::kerFailedToReadImageData);
  if (bufRead != header.size())
    throw Error(ErrorCode::kerInputDataReadFailed);

  DataBuf work(8);  // don't disturb the binary data - doWriteMetadata reuses it
  std::copy_n(header.c_data(), 8, work.begin());
  width = byteSwap_(work, 0, bSwap_);
  height = byteSwap_(work, 4, bSwap_);

  /* NOTE: properties not yet used
        byte nLevels  = buffer.pData_[8];
        byte quality  = buffer.pData_[9];
        byte bpp      = buffer.pData_[10];
        byte channels = buffer.pData_[11];
        */
  byte mode = header.read_uint8(12);

  if (mode == 2)  // Indexed color image. We pass color table (256 * 3 bytes).
  {
    header.alloc(16 + 256 * 3);

    bufRead = iIo.read(header.data(16), 256 * 3);
    if (iIo.error())
      throw Error(ErrorCode::kerFailedToReadImageData);
    if (bufRead != 256 * 3)
      throw Error(ErrorCode::kerInputDataReadFailed);
  }

  return header;
}  // PgfImage::readPgfHeaderStructure

Image::UniquePtr newPgfInstance(BasicIo::UniquePtr io, bool create) {
  auto image = std::make_unique<PgfImage>(std::move(io), create);
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isPgfType(BasicIo& iIo, bool advance) {
  const int32_t len = 3;
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  int rc = memcmp(buf, pgfSignature, 3);
  if (!advance || rc != 0) {
    iIo.seek(-len, BasicIo::cur);
  }

  return rc == 0;
}  // Exiv2::isPgfType
}  // namespace Exiv2
