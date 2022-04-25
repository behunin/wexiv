// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "enforce.hpp"
#include "error.hpp"
#include "helper_functions.hpp"
#include "image_int.hpp"
#include "jpgimage.hpp"
#include "photoshop.hpp"
#include "safe_op.hpp"

// *****************************************************************************

namespace Exiv2 {

namespace {
// JPEG Segment markers (The first byte is always 0xFF, the value of these constants correspond to the 2nd byte)
constexpr byte sos_ = 0xda;    //!< JPEG SOS marker
constexpr byte app0_ = 0xe0;   //!< JPEG APP0 marker
constexpr byte app1_ = 0xe1;   //!< JPEG APP1 marker
constexpr byte app2_ = 0xe2;   //!< JPEG APP2 marker
constexpr byte app13_ = 0xed;  //!< JPEG APP13 marker
constexpr byte com_ = 0xfe;    //!< JPEG Comment marker

// Markers without payload
constexpr byte soi_ = 0xd8;   ///!< SOI marker
constexpr byte eoi_ = 0xd9;   //!< JPEG EOI marker
constexpr byte rst1_ = 0xd0;  //!< JPEG Restart 0 Marker (from 0xD0 to 0xD7 there might be 8 of these markers)

// Start of Frame markers, nondifferential Huffman-coding frames
constexpr byte sof0_ = 0xc0;  //!< JPEG Start-Of-Frame marker
constexpr byte sof3_ = 0xc3;  //!< JPEG Start-Of-Frame marker

// Start of Frame markers, differential Huffman-coding frames
constexpr byte sof5_ = 0xc5;  //!< JPEG Start-Of-Frame marker

// Start of Frame markers, differential arithmetic-coding frames
constexpr byte sof15_ = 0xcf;  //!< JPEG Start-Of-Frame marker

constexpr auto exifId_ = "Exif\0\0";  //!< Exif identifier
// constexpr auto jfifId_ = "JFIF\0";                         //!< JFIF identifier
constexpr auto xmpId_ = "http://ns.adobe.com/xap/1.0/\0";  //!< XMP packet identifier
constexpr auto iccId_ = "ICC_PROFILE\0";                   //!< ICC profile identifier

inline bool inRange(int lo, int value, int hi) {
  return lo <= value && value <= hi;
}

inline bool inRange2(int value, int lo1, int hi1, int lo2, int hi2) {
  return inRange(lo1, value, hi1) || inRange(lo2, value, hi2);
}

/// @brief has the segment a non-zero payload?
/// @param m The marker at the start of a segment
/// @return true if the segment has a length field/payload
bool markerHasLength(byte m) {
  bool markerWithoutLength = m >= rst1_ && m <= eoi_;
  return !markerWithoutLength;
}

std::pair<std::array<byte, 2>, uint16_t> readSegmentSize(const byte marker, BasicIo& io) {
  std::array<byte, 2> buf{0, 0};  // 2-byte buffer for reading the size.
  uint16_t size{0};               // Size of the segment, including the 2-byte size field
  if (markerHasLength(marker)) {
    io.readOrThrow(buf.data(), buf.size(), ErrorCode::kerFailedToReadImageData);
    size = getUShort(buf.data(), bigEndian);
    enforce(size >= 2, ErrorCode::kerFailedToReadImageData);
  }
  return {buf, size};
}
}  // namespace

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
    throw Error(ErrorCode::kerDataSourceOpenFailed, io_->path(), strError());
  IoCloser closer(*io_);
  // Ensure that this is the correct image type
  if (!isThisType(*io_, true)) {
    if (io_->error() || io_->eof())
      throw Error(ErrorCode::kerFailedToReadImageData);
    throw Error(ErrorCode::kerNotAJpeg);
  }

  int search = 6;  // Exif, ICC, XMP, Comment, IPTC, SOF
  Blob psBlob;
  bool foundCompletePsData = false;
  bool foundExifData = false;
  bool foundXmpData = false;
  bool foundIccData = false;

  // Read section marker
  byte marker = advanceToMarker(ErrorCode::kerNotAJpeg);

  while (marker != sos_ && marker != eoi_ && search > 0) {
    auto [sizebuf, size] = readSegmentSize(marker, *io_);

    // Read the rest of the segment.
    DataBuf buf(size);
    /// \todo check if it makes sense to check for size
    if (size > 0) {
      io_->readOrThrow(buf.data(2), size - 2, ErrorCode::kerFailedToReadImageData);
      std::copy(sizebuf.begin(), sizebuf.end(), buf.begin());
    }

    if (!foundExifData && marker == app1_ && size >= 8  // prevent out-of-bounds read in memcmp on next line
        && buf.cmpBytes(2, exifId_, 6) == 0) {
      ByteOrder bo = ExifParser::decode(exifData_, buf.c_data(8), size - 8);
      setByteOrder(bo);
      if (size > 8 && byteOrder() == invalidByteOrder) {
        EXV_WARNING << "Failed to decode Exif metadata.";
      }
      --search;
      foundExifData = true;
    } else if (!foundXmpData && marker == app1_ && size >= 31  // prevent out-of-bounds read in memcmp on next line
               && buf.cmpBytes(2, xmpId_, 29) == 0) {
      xmpPacket_.assign(buf.c_str(31), size - 31);
      if (!xmpPacket_.empty() && XmpParser::decode(xmpData_, xmpPacket_)) {
        EXV_WARNING << "Failed to decode XMP metadata.";
      }
      --search;
      foundXmpData = true;
    } else if (!foundCompletePsData && marker == app13_ &&
               size >= 16  // prevent out-of-bounds read in memcmp on next line
               && buf.cmpBytes(2, Photoshop::ps3Id_, 14) == 0) {
      // Append to psBlob
      append(psBlob, buf.c_data(16), size - 16);
      // Check whether psBlob is complete
      if (!psBlob.empty() && Photoshop::valid(&psBlob[0], psBlob.size())) {
        --search;
        foundCompletePsData = true;
      }
    } else if (marker == com_ && comment_.empty()) {
      // JPEGs can have multiple comments, but for now only read
      // the first one (most jpegs only have one anyway). Comments
      // are simple single byte ISO-8859-1 strings.
      comment_.assign(buf.c_str(2), size - 2);
      while (comment_.length() && comment_.at(comment_.length() - 1) == '\0') {
        comment_.erase(comment_.length() - 1);
      }
      --search;
    } else if (marker == app2_ && size >= 13  // prevent out-of-bounds read in memcmp on next line
               && buf.cmpBytes(2, iccId_, 11) == 0) {
      if (size < 2 + 14 + 4) {
        rc = 8;
        break;
      }
      // ICC profile
      if (!foundIccData) {
        foundIccData = true;
        --search;
      }
      int chunk = static_cast<int>(buf.read_uint8(2 + 12));
      int chunks = static_cast<int>(buf.read_uint8(2 + 13));
      // ICC1v43_2010-12.pdf header is 14 bytes
      // header = "ICC_PROFILE\0" (12 bytes)
      // chunk/chunks are a single byte
      // Spec 7.2 Profile bytes 0-3 size
      uint32_t s = buf.read_uint32(2 + 14, bigEndian);
      // #1286 profile can be padded
      size_t icc_size = size - 2 - 14;
      if (chunk == 1 && chunks == 1) {
        enforce(s <= static_cast<uint32_t>(icc_size), ErrorCode::kerInvalidIccProfile);
        icc_size = s;
      }

      DataBuf profile(Safe::add(iccProfile_.size(), icc_size));
      if (!iccProfile_.empty()) {
        std::copy(iccProfile_.begin(), iccProfile_.end(), profile.begin());
      }
      std::copy_n(buf.c_data(2 + 14), icc_size, profile.data() + iccProfile_.size());
      setIccProfile(std::move(profile), chunk == chunks);
    } else if (pixelHeight_ == 0 && inRange2(marker, sof0_, sof3_, sof5_, sof15_)) {
      // We hit a SOFn (start-of-frame) marker
      if (size < 8) {
        rc = 7;
        break;
      }
      pixelHeight_ = buf.read_uint16(3, bigEndian);
      pixelWidth_ = buf.read_uint16(5, bigEndian);
      if (pixelHeight_ != 0)
        --search;
    }

    // Read the beginning of the next segment
    try {
      marker = advanceToMarker(ErrorCode::kerFailedToReadImageData);
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
    while (pCur < pEnd && 0 == Photoshop::locateIptcIrb(pCur, pEnd - pCur, &record, &sizeHdr, &sizeIptc)) {
      if (sizeIptc) {
        append(iptcBlob, record + sizeHdr, sizeIptc);
      }
      pCur = record + sizeHdr + sizeIptc + (sizeIptc & 1);
    }
    if (!iptcBlob.empty() && IptcParser::decode(iptcData_, &iptcBlob[0], iptcBlob.size())) {
      EXV_WARNING << "Failed to decode IPTC metadata.";
    }
  }

  if (rc != 0) {
    EXV_WARNING << "JPEG format error, rc = " << rc << "";
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
  auto image = std::make_unique<JpegImage>(std::move(io));
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
