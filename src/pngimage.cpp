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
#include "pngimage.hpp"

#include "enforce.hpp"
#include "error.hpp"
#include "pngchunk_int.hpp"
#include "tiffimage.hpp"
#include "types.hpp"

#include <zlib.h>  // To uncompress IccProfiles

// Signature from front of PNG file
const unsigned char pngSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

namespace Exiv2 {

using namespace Internal;

PngImage::PngImage(BasicIo::UniquePtr io) : Image(ImageType::png, mdExif | mdIptc | mdXmp | mdComment, std::move(io)) {
}  // PngImage::PngImage

std::string PngImage::mimeType() const {
  return "image/png";
}

static bool zlibToDataBuf(const byte* bytes, long length, DataBuf& result) {
  uLongf uncompressedLen = length * 2;  // just a starting point
  int zlibResult;

  do {
    result.alloc(uncompressedLen);
    zlibResult = uncompress(result.pData_, &uncompressedLen, bytes, length);
    // if result buffer is large than necessary, redo to fit perfectly.
    if (zlibResult == Z_OK && static_cast<long>(uncompressedLen) < result.size_) {
      result.reset();

      result.alloc(uncompressedLen);
      zlibResult = uncompress(result.pData_, &uncompressedLen, bytes, length);
    }
    if (zlibResult == Z_BUF_ERROR) {
      // the uncompressed buffer needs to be larger
      result.reset();

      // Sanity - never bigger than 16mb
      if (uncompressedLen > 16 * 1024 * 1024)
        zlibResult = Z_DATA_ERROR;
      else
        uncompressedLen *= 2;
    }
  } while (zlibResult == Z_BUF_ERROR);

  return zlibResult == Z_OK;
}

void readChunk(DataBuf& buffer, BasicIo& io) {
  long bufRead = io.read(buffer.pData_, buffer.size_);
  if (io.error()) {
    throw Error(kerFailedToReadImageData);
  }
  if (bufRead != buffer.size_) {
    throw Error(kerInputDataReadFailed);
  }
}  // Exiv2::readChunk

void PngImage::readMetadata() {
  if (io_->open() != 0) {
    throw Error(kerDataSourceOpenFailed, io_->path());
  }
  IoCloser closer(*io_);
  if (!isPngType(*io_, true)) {
    throw Error(kerNotAnImage, "PNG");
  }

  const long imgSize = static_cast<long>(io_->size());
  DataBuf cheaderBuf(8);  // Chunk header: 4 bytes (data size) + 4 bytes (chunk type).

  while (!io_->eof()) {
    std::memset(cheaderBuf.pData_, 0x0, cheaderBuf.size_);
    readChunk(cheaderBuf, *io_);  // Read chunk header.

    // Decode chunk data length.
    uint32_t chunkLength = Exiv2::getULong(cheaderBuf.pData_, Exiv2::bigEndian);
    long pos = io_->tell();
    if (pos == -1 || chunkLength > uint32_t(0x7FFFFFFF) || static_cast<long>(chunkLength) > imgSize - pos) {
      throw Exiv2::Error(kerFailedToReadImageData);
    }

    std::string chunkType(reinterpret_cast<char*>(cheaderBuf.pData_) + 4, 4);

    /// \todo analyse remaining chunks of the standard
    // Perform a chunk triage for item that we need.
    if (chunkType == "IEND" || chunkType == "IHDR" || chunkType == "tEXt" || chunkType == "zTXt" ||
        chunkType == "eXIf" || chunkType == "iTXt" || chunkType == "iCCP") {
      DataBuf chunkData(chunkLength);
      readChunk(chunkData, *io_);  // Extract chunk data.

      if (chunkType == "IEND") {
        return;  // Last chunk found: we stop parsing.
      }
      if (chunkType == "IHDR" && chunkData.size_ >= 8) {
        PngChunk::decodeIHDRChunk(chunkData, &pixelWidth_, &pixelHeight_);
      } else if (chunkType == "tEXt") {
        PngChunk::decodeTXTChunk(this, chunkData, PngChunk::tEXt_Chunk);
      } else if (chunkType == "zTXt") {
        PngChunk::decodeTXTChunk(this, chunkData, PngChunk::zTXt_Chunk);
      } else if (chunkType == "iTXt") {
        PngChunk::decodeTXTChunk(this, chunkData, PngChunk::iTXt_Chunk);
      } else if (chunkType == "eXIf") {
        ByteOrder bo = TiffParser::decode(exifData(), iptcData(), xmpData(), chunkData.pData_, chunkData.size_);
        setByteOrder(bo);
      } else if (chunkType == "iCCP") {
        // The ICC profile name can vary from 1-79 characters.
        uint32_t iccOffset = 0;
        do {
          enforce(iccOffset < 80 && iccOffset < chunkLength, Exiv2::kerCorruptedMetadata);
        } while (chunkData.pData_[iccOffset++] != 0x00);

        profileName_ = std::string(reinterpret_cast<char*>(chunkData.pData_), iccOffset - 1);
        ++iccOffset;  // +1 = 'compressed' flag
        enforce(iccOffset <= chunkLength, Exiv2::kerCorruptedMetadata);

        zlibToDataBuf(chunkData.pData_ + iccOffset, chunkLength - iccOffset, iccProfile_);
      }

      // Set chunkLength to 0 in case we have read a supported chunk type. Otherwise, we need to seek the
      // file to the next chunk position.
      chunkLength = 0;
    }

    // Move to the next chunk: chunk data size + 4 CRC bytes.
    io_->seek(chunkLength + 4, BasicIo::cur);
    if (io_->error() || io_->eof()) {
      throw Error(kerFailedToReadImageData);
    }
  }
}  // PngImage::readMetadata

Image::UniquePtr newPngInstance(BasicIo::UniquePtr io, bool create) {
  Image::UniquePtr image(new PngImage(std::move(io)));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isPngType(BasicIo& iIo, bool advance) {
  if (iIo.error() || iIo.eof()) {
    throw Error(kerInputDataReadFailed);
  }
  const int32_t len = 8;
  byte buf[len];
  iIo.read(buf, len);
  if (iIo.error() || iIo.eof()) {
    return false;
  }
  int rc = memcmp(buf, pngSignature, 8);
  if (!advance || rc != 0) {
    iIo.seek(-len, BasicIo::cur);
  }

  return rc == 0;
}  // Exiv2::isPngType
}  // namespace Exiv2
