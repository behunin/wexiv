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
#include "jpgimage.hpp"

#include "config.h"
#include "enforce.hpp"
#include "error.hpp"
#include "helper_functions.hpp"
#include "image_int.hpp"
#include "safe_op.hpp"
#include "tiffimage.hpp"

// *****************************************************************************

namespace Exiv2 {
constexpr byte JpegBase::dht_ = 0xc4;
constexpr byte JpegBase::dqt_ = 0xdb;
constexpr byte JpegBase::dri_ = 0xdd;
constexpr byte JpegBase::sos_ = 0xda;
constexpr byte JpegBase::eoi_ = 0xd9;
constexpr byte JpegBase::app0_ = 0xe0;
constexpr byte JpegBase::app1_ = 0xe1;
constexpr byte JpegBase::app2_ = 0xe2;
constexpr byte JpegBase::app13_ = 0xed;
constexpr byte JpegBase::com_ = 0xfe;

// Start of Frame markers, nondifferential Huffman-coding frames
constexpr byte JpegBase::sof0_ = 0xc0;  // start of frame 0, baseline DCT
constexpr byte JpegBase::sof1_ = 0xc1;  // start of frame 1, extended sequential DCT, Huffman coding
constexpr byte JpegBase::sof2_ = 0xc2;  // start of frame 2, progressive DCT, Huffman coding
constexpr byte JpegBase::sof3_ = 0xc3;  // start of frame 3, lossless sequential, Huffman coding

// Start of Frame markers, differential Huffman-coding frames
constexpr byte JpegBase::sof5_ = 0xc5;  // start of frame 5, differential sequential DCT, Huffman coding
constexpr byte JpegBase::sof6_ = 0xc6;  // start of frame 6, differential progressive DCT, Huffman coding
constexpr byte JpegBase::sof7_ = 0xc7;  // start of frame 7, differential lossless, Huffman coding

// Start of Frame markers, nondifferential arithmetic-coding frames
constexpr byte JpegBase::sof9_ = 0xc9;   // start of frame 9, extended sequential DCT, arithmetic coding
constexpr byte JpegBase::sof10_ = 0xca;  // start of frame 10, progressive DCT, arithmetic coding
constexpr byte JpegBase::sof11_ = 0xcb;  // start of frame 11, lossless sequential, arithmetic coding

// Start of Frame markers, differential arithmetic-coding frames
constexpr byte JpegBase::sof13_ = 0xcd;  // start of frame 13, differential sequential DCT, arithmetic coding
constexpr byte JpegBase::sof14_ = 0xce;  // start of frame 14, progressive DCT, arithmetic coding
constexpr byte JpegBase::sof15_ = 0xcf;  // start of frame 15, differential lossless, arithmetic coding

constexpr const char* JpegBase::exifId_ = "Exif\0\0";
constexpr const char* JpegBase::jfifId_ = "JFIF\0";
constexpr const char* JpegBase::xmpId_ = "http://ns.adobe.com/xap/1.0/\0";
constexpr const char* JpegBase::iccId_ = "ICC_PROFILE\0";

constexpr const char* Photoshop::ps3Id_ = "Photoshop 3.0\0";
constexpr std::array<const char*, 4> Photoshop::irbId_{"8BIM", "AgHg", "DCSR", "PHUT"};
constexpr const char* Photoshop::bimId_ = "8BIM";  // deprecated
constexpr uint16_t Photoshop::iptc_ = 0x0404;
constexpr uint16_t Photoshop::preview_ = 0x040c;

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

static inline bool inRange(int lo, int value, int hi) {
  return lo <= value && value <= hi;
}

static inline bool inRange2(int value, int lo1, int hi1, int lo2, int hi2) {
  return inRange(lo1, value, hi1) || inRange(lo2, value, hi2);
}

bool Photoshop::isIrb(const byte* pPsData, long sizePsData) {
  if (sizePsData < 4)
    return false;
  for (auto&& i : irbId_) {
    assert(strlen(i) == 4);
    if (memcmp(pPsData, i, 4) == 0)
      return true;
  }
  return false;
}

bool Photoshop::valid(const byte* pPsData, long sizePsData) {
  const byte* record = nullptr;
  uint32_t sizeIptc = 0;
  uint32_t sizeHdr = 0;
  const byte* pCur = pPsData;
  const byte* pEnd = pPsData + sizePsData;
  int ret = 0;
  while (pCur < pEnd &&
         0 == (ret = Photoshop::locateIptcIrb(pCur, static_cast<long>(pEnd - pCur), &record, &sizeHdr, &sizeIptc))) {
    pCur = record + sizeHdr + sizeIptc + (sizeIptc & 1);
  }
  return ret >= 0;
}

// Todo: Generalised from JpegBase::locateIptcData without really understanding
//       the format (in particular the header). So it remains to be confirmed
//       if this also makes sense for psTag != Photoshop::iptc
int Photoshop::locateIrb(const byte* pPsData, long sizePsData, uint16_t psTag, const byte** record,
                         uint32_t* const sizeHdr, uint32_t* const sizeData) {
  assert(record);
  assert(sizeHdr);
  assert(sizeData);
  // Used for error checking
  long position = 0;
  // Data should follow Photoshop format, if not exit
  while (position <= sizePsData - 12 && isIrb(pPsData + position, 4)) {
    const byte* hrd = pPsData + position;
    position += 4;
    uint16_t type = getUShort(pPsData + position, bigEndian);
    position += 2;
    // Pascal string is padded to have an even size (including size byte)
    byte psSize = pPsData[position] + 1;
    psSize += (psSize & 1);
    position += psSize;
    if (position + 4 > sizePsData) {
#ifdef EXIV2_DEBUG_MESSAGES
      std::cerr << "Warning: "
                << "Invalid or extended Photoshop IRB\n";
#endif
      return -2;
    }
    uint32_t dataSize = getULong(pPsData + position, bigEndian);
    position += 4;
    if (dataSize > static_cast<uint32_t>(sizePsData - position)) {
#ifdef EXIV2_DEBUG_MESSAGES
      std::cerr << "Warning: "
                << "Invalid Photoshop IRB data size " << dataSize << " or extended Photoshop IRB\n";
#endif
      return -2;
    }
#ifdef EXIV2_DEBUG_MESSAGES
    if ((dataSize & 1) && position + dataSize == static_cast<uint32_t>(sizePsData)) {
      std::cerr << "Warning: "
                << "Photoshop IRB data is not padded to even size\n";
    }
#endif
    if (type == psTag) {
      *sizeData = dataSize;
      *sizeHdr = psSize + 10;
      *record = hrd;
      return 0;
    }
    // Data size is also padded to be even
    position += dataSize + (dataSize & 1);
  }
  if (position < sizePsData) {
    return -2;
  }
  return 3;
}  // Photoshop::locateIrb

int Photoshop::locateIptcIrb(const byte* pPsData, long sizePsData, const byte** record, uint32_t* const sizeHdr,
                             uint32_t* const sizeData) {
  return locateIrb(pPsData, sizePsData, iptc_, record, sizeHdr, sizeData);
}

int Photoshop::locatePreviewIrb(const byte* pPsData, long sizePsData, const byte** record, uint32_t* const sizeHdr,
                                uint32_t* const sizeData) {
  return locateIrb(pPsData, sizePsData, preview_, record, sizeHdr, sizeData);
}

DataBuf Photoshop::setIptcIrb(const byte* pPsData, long sizePsData, const IptcData& iptcData) {
  if (sizePsData > 0)
    assert(pPsData);
  const byte* record = pPsData;
  uint32_t sizeIptc = 0;
  uint32_t sizeHdr = 0;
  DataBuf rc;
  // Safe to call with zero psData.size_
  if (0 > Photoshop::locateIptcIrb(pPsData, sizePsData, &record, &sizeHdr, &sizeIptc)) {
    return rc;
  }
  Blob psBlob;
  const auto sizeFront = static_cast<uint32_t>(record - pPsData);
  // Write data before old record.
  if (sizePsData > 0 && sizeFront > 0) {
    append(psBlob, pPsData, sizeFront);
  }
  // Write new iptc record if we have it

  // Write existing stuff after record,
  // skip the current and all remaining IPTC blocks
  long pos = sizeFront;
  while (0 == Photoshop::locateIptcIrb(pPsData + pos, sizePsData - pos, &record, &sizeHdr, &sizeIptc)) {
    const long newPos = static_cast<long>(record - pPsData);
    // Copy data up to the IPTC IRB
    if (newPos > pos) {
      append(psBlob, pPsData + pos, newPos - pos);
    }
    // Skip the IPTC IRB
    pos = newPos + sizeHdr + sizeIptc + (sizeIptc & 1);
  }
  if (pos < sizePsData) {
    append(psBlob, pPsData + pos, sizePsData - pos);
  }
  // Data is rounded to be even
  if (!psBlob.empty())
    rc = DataBuf(&psBlob[0], static_cast<long>(psBlob.size()));

  return rc;

}  // Photoshop::setIptcIrb

bool JpegBase::markerHasLength(byte marker) {
  return (marker >= sof0_ && marker <= sof15_) || (marker >= app0_ && marker <= (app0_ | 0x0F)) || marker == dht_ ||
         marker == dqt_ || marker == dri_ || marker == com_ || marker == sos_;
}

JpegBase::JpegBase(int type, BasicIo::UniquePtr io) : Image(type, mdExif | mdIptc | mdXmp | mdComment, std::move(io)) {
}

byte JpegBase::advanceToMarker(ErrorCode err) const {
  int c = -1;
  // Skips potential padding between markers
  while ((c = io_->getb()) != 0xff) {
    if (c == EOF)
      throw Error(err);
  }

  // Markers can start with any number of 0xff
  while ((c = io_->getb()) == 0xff) {
  }
  if (c == EOF)
    throw Error(err);

  return static_cast<byte>(c);
}

void JpegBase::readMetadata() {
  int rc = 0;  // Todo: this should be the return value

  if (io_->open() != 0)
    throw Error(kerDataSourceOpenFailed, io_->path());
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isThisType(*io_, true)) {
    if (io_->error() || io_->eof())
      throw Error(kerFailedToReadImageData);
    throw Error(kerNotAJpeg);
  }

  int search = 6;  // Exif, ICC, XMP, Comment, IPTC, SOF
  Blob psBlob;
  bool foundCompletePsData = false;
  bool foundExifData = false;
  bool foundXmpData = false;
  bool foundIccData = false;

  // Read section marker
  byte marker = advanceToMarker(kerNotAJpeg);

  while (marker != sos_ && marker != eoi_ && search > 0) {
    // 2-byte buffer for reading the size.
    byte sizebuf[2];
    uint16_t size = 0;
    if (markerHasLength(marker)) {
      readOrThrow(*io_, sizebuf, 2, kerFailedToReadImageData);
      size = getUShort(sizebuf, bigEndian);
      // `size` is the size of the segment, including the 2-byte size field
      // that we just read.
      enforce(size >= 2, kerFailedToReadImageData);
    }

    // Read the rest of the segment.
    DataBuf buf(size);
    if (size > 0) {
      readOrThrow(*io_, buf.pData_ + 2, size - 2, kerFailedToReadImageData);
      memcpy(buf.pData_, sizebuf, 2);
    }

    if (!foundExifData && marker == app1_ && size >= 8  // prevent out-of-bounds read in memcmp on next line
        && memcmp(buf.pData_ + 2, exifId_, 6) == 0) {
      ByteOrder bo = ExifParser::decode(exifData_, buf.pData_ + 8, size - 8);
      setByteOrder(bo);
      if (size > 8 && byteOrder() == invalidByteOrder) {
#ifndef SUPPRESS_WARNINGS
        EXV_WARNING << "Failed to decode Exif metadata.\n";
#endif
      }
      --search;
      foundExifData = true;
    } else if (!foundXmpData && marker == app1_ && size >= 31  // prevent out-of-bounds read in memcmp on next line
               && memcmp(buf.pData_ + 2, xmpId_, 29) == 0) {
      xmpPacket_.assign(reinterpret_cast<char*>(buf.pData_ + 31), size - 31);
      if (!xmpPacket_.empty() && XmpParser::decode(xmpData_, xmpPacket_)) {
#ifndef SUPPRESS_WARNINGS
        EXV_WARNING << "Failed to decode XMP metadata.\n";
#endif
      }
      --search;
      foundXmpData = true;
    } else if (!foundCompletePsData && marker == app13_ &&
               size >= 16  // prevent out-of-bounds read in memcmp on next line
               && memcmp(buf.pData_ + 2, Photoshop::ps3Id_, 14) == 0) {
      // Append to psBlob
      append(psBlob, buf.pData_ + 16, size - 16);
      // Check whether psBlob is complete
      if (!psBlob.empty() && Photoshop::valid(&psBlob[0], static_cast<long>(psBlob.size()))) {
        --search;
        foundCompletePsData = true;
      }
    } else if (marker == com_ && comment_.empty()) {
      // JPEGs can have multiple comments, but for now only read
      // the first one (most jpegs only have one anyway). Comments
      // are simple single byte ISO-8859-1 strings.
      comment_.assign(reinterpret_cast<char*>(buf.pData_ + 2), size - 2);
      while (comment_.length() && comment_.at(comment_.length() - 1) == '\0') {
        comment_.erase(comment_.length() - 1);
      }
      --search;
    } else if (marker == app2_ && size >= 13  // prevent out-of-bounds read in memcmp on next line
               && memcmp(buf.pData_ + 2, iccId_, 11) == 0) {
      if (size < 2 + 14 + 4) {
        rc = 8;
        break;
      }
      // ICC profile
      if (!foundIccData) {
        foundIccData = true;
        --search;
      }
      int chunk = static_cast<int>(buf.pData_[2 + 12]);
      int chunks = static_cast<int>(buf.pData_[2 + 13]);
      // ICC1v43_2010-12.pdf header is 14 bytes
      // header = "ICC_PROFILE\0" (12 bytes)
      // chunk/chunks are a single byte
      // Spec 7.2 Profile bytes 0-3 size
      uint32_t s = getULong(buf.pData_ + (2 + 14), bigEndian);
      // #1286 profile can be padded
      long icc_size = size - 2 - 14;
      if (chunk == 1 && chunks == 1) {
        enforce(s <= static_cast<uint32_t>(icc_size), kerInvalidIccProfile);
        icc_size = s;
      }

      DataBuf profile(Safe::add(iccProfile_.size_, icc_size));
      if (iccProfile_.size_) {
        ::memcpy(profile.pData_, iccProfile_.pData_, iccProfile_.size_);
      }
      ::memcpy(profile.pData_ + iccProfile_.size_, buf.pData_ + (2 + 14), icc_size);
      // setIccProfile(profile, chunk == chunks);
    } else if (pixelHeight_ == 0 && inRange2(marker, sof0_, sof3_, sof5_, sof15_)) {
      // We hit a SOFn (start-of-frame) marker
      if (size < 8) {
        rc = 7;
        break;
      }
      pixelHeight_ = getUShort(buf.pData_ + 3, bigEndian);
      pixelWidth_ = getUShort(buf.pData_ + 5, bigEndian);
      if (pixelHeight_ != 0)
        --search;
    }

    // Read the beginning of the next segment
    try {
      marker = advanceToMarker(kerFailedToReadImageData);
    } catch (Error&) {
      rc = 5;
      break;
    }
  }  // while there are segments to process

  if (!psBlob.empty()) {
    // Find actual IPTC data within the psBlob
    Blob iptcBlob;
    const byte* record = nullptr;
    uint32_t sizeIptc = 0;
    uint32_t sizeHdr = 0;
    const byte* pCur = &psBlob[0];
    const byte* pEnd = pCur + psBlob.size();
    while (pCur < pEnd &&
           0 == Photoshop::locateIptcIrb(pCur, static_cast<long>(pEnd - pCur), &record, &sizeHdr, &sizeIptc)) {
      if (sizeIptc) {
        append(iptcBlob, record + sizeHdr, sizeIptc);
      }
      pCur = record + sizeHdr + sizeIptc + (sizeIptc & 1);
    }
    if (!iptcBlob.empty() && IptcParser::decode(iptcData_, &iptcBlob[0], static_cast<uint32_t>(iptcBlob.size()))) {
#ifndef SUPPRESS_WARNINGS
      EXV_WARNING << "Failed to decode IPTC metadata.\n";
#endif
    }
  }  // psBlob.size() > 0

  if (rc != 0) {
#ifndef SUPPRESS_WARNINGS
    EXV_WARNING << "JPEG format error, rc = " << rc << "\n";
#endif
  }
}  // JpegBase::readMetadata

#define REPORT_MARKER                                 \
  if ((option == kpsBasic || option == kpsRecursive)) \
  out << Internal::stringFormat("%8ld | 0xff%02x %-5s", io_->tell() - 2, marker, nm[marker].c_str())

const byte JpegImage::soi_ = 0xd8;

JpegImage::JpegImage(BasicIo::UniquePtr io) : JpegBase(ImageType::jpeg, std::move(io)) {
}

std::string JpegImage::mimeType() const {
  return "image/jpeg";
}

bool JpegImage::isThisType(BasicIo& iIo, bool advance) const {
  return isJpegType(iIo, advance);
}

Image::UniquePtr newJpegInstance(BasicIo::UniquePtr io, bool create) {
  Image::UniquePtr image(new JpegImage(std::move(io)));
  if (!image->good()) {
    image.reset();
  }
  return image;
}

bool isJpegType(BasicIo& iIo, bool advance) {
  bool result = true;
  byte tmpBuf[2];
  iIo.read(tmpBuf, 2);
  if (iIo.error() || iIo.eof())
    return false;

  if (0xff != tmpBuf[0] || JpegImage::soi_ != tmpBuf[1]) {
    result = false;
  }
  if (!advance || !result)
    iIo.seek(-2, BasicIo::cur);
  return result;
}

}  // namespace Exiv2
