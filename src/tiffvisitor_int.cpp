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
#include "tiffvisitor_int.hpp"  // see bug #487

#include "config.h"
#include "enforce.hpp"
#include "exif.hpp"
#include "i18n.h"  // NLS support.
#include "image.hpp"
#include "image_int.hpp"
#include "iptc.hpp"
#include "jpgimage.hpp"
#include "makernote_int.hpp"
#include "sonymn_int.hpp"
#include "tiffcomposite_int.hpp"  // Do not change the order of these 2 includes,
#include "tiffimage_int.hpp"
#include "value.hpp"

// *****************************************************************************
namespace {
//! Unary predicate that matches an Exifdatum with a given group and index.
class FindExifdatum2 {
 public:
  //! Constructor, initializes the object with the group and index to look for.
  FindExifdatum2(Exiv2::Internal::IfdId group, int idx) : groupName_(Exiv2::Internal::groupName(group)), idx_(idx) {
  }
  //! Returns true if group and index match.
  bool operator()(const Exiv2::Exifdatum& md) const {
    return idx_ == md.idx() && 0 == strcmp(md.groupName().c_str(), groupName_);
  }

 private:
  const char* groupName_;
  int idx_;

};  // class FindExifdatum2

Exiv2::ByteOrder stringToByteOrder(const std::string& val) {
  Exiv2::ByteOrder bo = Exiv2::invalidByteOrder;
  if (0 == strcmp("II", val.c_str()))
    bo = Exiv2::littleEndian;
  else if (0 == strcmp("MM", val.c_str()))
    bo = Exiv2::bigEndian;

  return bo;
}
}  // namespace

// *****************************************************************************
// class member definitions
namespace Exiv2 {
namespace Internal {

TiffVisitor::TiffVisitor() {
  go_.fill(true);
}

void TiffVisitor::setGo(GoEvent event, bool go) {
  assert(event >= 0 && static_cast<int>(event) < events_);
  go_[event] = go;
}

bool TiffVisitor::go(GoEvent event) const {
  assert(event >= 0 && static_cast<int>(event) < events_);
  return go_[event];
}

void TiffVisitor::visitDirectoryNext(TiffDirectory* /*object*/) {
}

void TiffVisitor::visitDirectoryEnd(TiffDirectory* /*object*/) {
}

void TiffVisitor::visitIfdMakernoteEnd(TiffIfdMakernote* /*object*/) {
}

void TiffVisitor::visitBinaryArrayEnd(TiffBinaryArray* /*object*/) {
}

void TiffFinder::init(uint16_t tag, IfdId group) {
  tag_ = tag;
  group_ = group;
  tiffComponent_ = nullptr;
  setGo(geTraverse, true);
}

void TiffFinder::findObject(TiffComponent* object) {
  if (object->tag() == tag_ && object->group() == group_) {
    tiffComponent_ = object;
    setGo(geTraverse, false);
  }
}

void TiffFinder::visitEntry(TiffEntry* object) {
  findObject(object);
}

void TiffFinder::visitDataEntry(TiffDataEntry* object) {
  findObject(object);
}

void TiffFinder::visitImageEntry(TiffImageEntry* object) {
  findObject(object);
}

void TiffFinder::visitSizeEntry(TiffSizeEntry* object) {
  findObject(object);
}

void TiffFinder::visitDirectory(TiffDirectory* object) {
  findObject(object);
}

void TiffFinder::visitSubIfd(TiffSubIfd* object) {
  findObject(object);
}

void TiffFinder::visitMnEntry(TiffMnEntry* object) {
  findObject(object);
}

void TiffFinder::visitIfdMakernote(TiffIfdMakernote* object) {
  findObject(object);
}

void TiffFinder::visitBinaryArray(TiffBinaryArray* object) {
  findObject(object);
}

void TiffFinder::visitBinaryElement(TiffBinaryElement* object) {
  findObject(object);
}

void TiffCopier::visitEntry(TiffEntry* object) {
  copyObject(object);
}

void TiffCopier::visitDataEntry(TiffDataEntry* object) {
  copyObject(object);
}

void TiffCopier::visitImageEntry(TiffImageEntry* object) {
  copyObject(object);
}

void TiffCopier::visitSizeEntry(TiffSizeEntry* object) {
  copyObject(object);
}

void TiffCopier::visitDirectory(TiffDirectory* /*object*/) {
  // Do not copy directories (avoids problems with SubIfds)
}

void TiffCopier::visitSubIfd(TiffSubIfd* object) {
  copyObject(object);
}

void TiffCopier::visitMnEntry(TiffMnEntry* object) {
  copyObject(object);
}

void TiffCopier::visitIfdMakernote(TiffIfdMakernote* object) {
  copyObject(object);
}

void TiffCopier::visitBinaryArray(TiffBinaryArray* object) {
  copyObject(object);
}

void TiffCopier::visitBinaryElement(TiffBinaryElement* object) {
  copyObject(object);
}

TiffDecoder::TiffDecoder(emscripten::val& exifData, emscripten::val& iptcData, emscripten::val& xmpData,
                         TiffComponent* const pRoot, FindDecoderFct findDecoderFct) :
    exifData_(exifData),
    iptcData_(iptcData),
    xmpData_(xmpData),
    pRoot_(pRoot),
    findDecoderFct_(findDecoderFct),
    decodedIptc_(false) {
  assert(pRoot != 0);

  // #1402 Fujifilm RAF. Search for the make
  // Find camera make in existing metadata (read from the JPEG)
  const char* key("Exif.Image.Make");
  if (exifData_.hasOwnProperty(key)) {
    make_ = exifData_[key].as<std::string>();
  } else {
    // Find camera make by looking for tag 0x010f in IFD0
    TiffFinder finder(0x010f, ifd0Id);
    pRoot_->accept(finder);
    auto te = dynamic_cast<TiffEntryBase*>(finder.result());
    if (te && te->pValue()) {
      make_ = te->pValue()->toString();
    }
  }
}

void TiffDecoder::visitEntry(TiffEntry* object) {
  decodeTiffEntry(object);
}

void TiffDecoder::visitDataEntry(TiffDataEntry* object) {
  decodeTiffEntry(object);
}

void TiffDecoder::visitImageEntry(TiffImageEntry* object) {
  decodeTiffEntry(object);
}

void TiffDecoder::visitSizeEntry(TiffSizeEntry* object) {
  decodeTiffEntry(object);
}

void TiffDecoder::visitDirectory(TiffDirectory* /* object */) {
  // Nothing to do
}

void TiffDecoder::visitSubIfd(TiffSubIfd* object) {
  decodeTiffEntry(object);
}

void TiffDecoder::visitMnEntry(TiffMnEntry* object) {
  // Always decode binary makernote tag
  decodeTiffEntry(object);
}

void TiffDecoder::visitIfdMakernote(TiffIfdMakernote* object) {
  assert(object != 0);

  exifData_.set("Exif.MakerNote.Offset", object->mnOffset());
  switch (object->byteOrder()) {
    case littleEndian:
      exifData_.set("Exif.MakerNote.ByteOrder", "II");
      break;
    case bigEndian:
      exifData_.set("Exif.MakerNote.ByteOrder", "MM");
      break;
    case invalidByteOrder:
      assert(object->byteOrder() != invalidByteOrder);
      break;
  }
}

void TiffDecoder::getObjData(byte const*& pData, long& size, uint16_t tag, IfdId group, const TiffEntryBase* object) {
  if (object && object->tag() == tag && object->group() == group) {
    pData = object->pData();
    size = object->size();
    return;
  }
  TiffFinder finder(tag, group);
  pRoot_->accept(finder);
  TiffEntryBase const* te = dynamic_cast<TiffEntryBase*>(finder.result());
  if (te) {
    pData = te->pData();
    size = te->size();
    return;
  }
}

void TiffDecoder::decodeXmp(const TiffEntryBase* object) {
  // add Exif tag anyway
  decodeStdTiffEntry(object);

  byte const* pData = nullptr;
  long size = 0;
  getObjData(pData, size, 0x02bc, ifd0Id, object);
  if (pData) {
    std::string xmpPacket;
    xmpPacket.assign(reinterpret_cast<const char*>(pData), size);
    std::string::size_type idx = xmpPacket.find_first_of('<');
    if (idx != std::string::npos && idx > 0) {
#ifndef SUPPRESS_WARNINGS
      EXV_WARNING << "Removing " << static_cast<unsigned long>(idx)
                  << " characters from the beginning of the XMP packet\n";
#endif
      xmpPacket = xmpPacket.substr(idx);
    }
    if (XmpParser::decode(xmpData_, xmpPacket)) {
#ifndef SUPPRESS_WARNINGS
      EXV_WARNING << "Failed to decode XMP metadata.\n";
#endif
    }
  }
}  // TiffDecoder::decodeXmp

void TiffDecoder::decodeIptc(const TiffEntryBase* object) {
  // add Exif tag anyway
  decodeStdTiffEntry(object);

  // All tags are read at this point, so the first time we come here,
  // find the relevant IPTC tag and decode IPTC if found
  if (decodedIptc_) {
    return;
  }
  decodedIptc_ = true;
  // 1st choice: IPTCNAA
  byte const* pData = nullptr;
  long size = 0;
  getObjData(pData, size, 0x83bb, ifd0Id, object);
  if (pData) {
    if (0 == IptcParser::decode(iptcData_, pData, size)) {
      return;
    }
#ifndef SUPPRESS_WARNINGS
    EXV_WARNING << "Failed to decode IPTC block found in "
                << "Directory Image, entry 0x83bb\n";

#endif
  }

  // 2nd choice if no IPTCNAA record found or failed to decode it:
  // ImageResources
  pData = nullptr;
  size = 0;
  getObjData(pData, size, 0x8649, ifd0Id, object);
  if (pData) {
    byte const* record = nullptr;
    uint32_t sizeHdr = 0;
    uint32_t sizeData = 0;
    if (0 != Photoshop::locateIptcIrb(pData, size, &record, &sizeHdr, &sizeData)) {
      return;
    }
    if (0 == IptcParser::decode(iptcData_, record + sizeHdr, sizeData)) {
      return;
    }
#ifndef SUPPRESS_WARNINGS
    EXV_WARNING << "Failed to decode IPTC block found in "
                << "Directory Image, entry 0x8649\n";

#endif
  }
}  // TiffMetadataDecoder::decodeIptc

static const TagInfo* findTag(const TagInfo* pList, uint16_t tag) {
  while (pList->tag_ != 0xffff && pList->tag_ != tag)
    pList++;
  return pList->tag_ != 0xffff ? pList : nullptr;
}

void TiffDecoder::decodeCanonAFInfo(const TiffEntryBase* object) {
  // report Exif.Canon.AFInfo as usual
  TiffDecoder::decodeStdTiffEntry(object);
  if (object->pValue()->count() < 3 || object->pValue()->typeId() != unsignedShort)
    return;  // insufficient data

  // create vector of signedShorts from unsignedShorts in Exif.Canon.AFInfo
  std::vector<int16_t> ints;
  std::vector<uint16_t> uint;
  for (long i = 0; i < object->pValue()->count(); i++) {
    ints.push_back(static_cast<int16_t>(object->pValue()->toLong(i)));
    uint.push_back(static_cast<uint16_t>(object->pValue()->toLong(i)));
  }
  // Check this is AFInfo2 (ints[0] = bytes in object)
  if (ints.at(0) != object->pValue()->count() * 2)
    return;

  std::string familyGroup(std::string("Exif.") + groupName(object->group()) + ".");

  const uint16_t nPoints = uint.at(2);
  const uint16_t nMasks = (nPoints + 15) / (sizeof(uint16_t) * 8);
  int nStart = 0;

  struct {
    uint16_t tag;
    uint16_t size;
    bool bSigned;
  } records[] = {
      {0x2600, 1, true},        // AFInfoSize
      {0x2601, 1, true},        // AFAreaMode
      {0x2602, 1, true},        // AFNumPoints
      {0x2603, 1, true},        // AFValidPoints
      {0x2604, 1, true},        // AFCanonImageWidth
      {0x2605, 1, true},        // AFCanonImageHeight
      {0x2606, 1, true},        // AFImageWidth"
      {0x2607, 1, true},        // AFImageHeight
      {0x2608, nPoints, true},  // AFAreaWidths
      {0x2609, nPoints, true},  // AFAreaHeights
      {0x260a, nPoints, true},  // AFXPositions
      {0x260b, nPoints, true},  // AFYPositions
      {0x260c, nMasks, false},  // AFPointsInFocus
      {0x260d, nMasks, false},  // AFPointsSelected
      {0x260e, nMasks, false},  // AFPointsUnusable
  };
  // check we have enough data!
  uint16_t count = 0;
  for (auto&& record : records) {
    count += record.size;
    if (count > ints.size())
      return;
  }

  for (auto&& record : records) {
    const TagInfo* pTags = ExifTags::tagList("Canon");
    const TagInfo* pTag = findTag(pTags, record.tag);
    if (pTag) {
      auto v = Exiv2::Value::create(record.bSigned ? Exiv2::signedShort : Exiv2::unsignedShort);
      std::ostringstream s;
      if (record.bSigned) {
        for (uint16_t k = 0; k < record.size; k++)
          s << " " << ints.at(nStart++);
      } else {
        for (uint16_t k = 0; k < record.size; k++)
          s << " " << uint.at(nStart++);
      }

      v->read(s.str());
      exifData_.set(familyGroup + pTag->name_, v->toString());
    }
  }
}

void TiffDecoder::decodeTiffEntry(const TiffEntryBase* object) {
  assert(object != 0);

  // Don't decode the entry if value is not set
  if (!object->pValue())
    return;

  const DecoderFct decoderFct = findDecoderFct_(make_, object->tag(), object->group());
  // skip decoding if decoderFct == 0
  if (decoderFct) {
    EXV_CALL_MEMBER_FN(*this, decoderFct)(object);
  }
}  // TiffDecoder::decodeTiffEntry

void TiffDecoder::decodeStdTiffEntry(const TiffEntryBase* object) {
  assert(object != 0);
  ExifKey key(object->tag(), groupName(object->group()));
  key.setIdx(object->idx());
  exifData_.set(key.key(), object->pValue()->toString());

}  // TiffDecoder::decodeTiffEntry

void TiffDecoder::visitBinaryArray(TiffBinaryArray* object) {
  if (object->cfg() == nullptr || !object->decoded()) {
    decodeTiffEntry(object);
  }
}

void TiffDecoder::visitBinaryElement(TiffBinaryElement* object) {
  decodeTiffEntry(object);
}

TiffReader::TiffReader(const byte* pData, uint32_t size, TiffComponent* pRoot, TiffRwState state) :
    pData_(pData),
    size_(size),
    pLast_(pData + size),
    pRoot_(pRoot),
    origState_(state),
    mnState_(state),
    postProc_(false) {
  pState_ = &origState_;
  assert(pData_);
  assert(size_ > 0);

}  // TiffReader::TiffReader

void TiffReader::setOrigState() {
  pState_ = &origState_;
}

void TiffReader::setMnState(const TiffRwState* state) {
  if (state != nullptr) {
    // invalidByteOrder indicates 'no change'
    if (state->byteOrder() == invalidByteOrder) {
      mnState_ = TiffRwState(origState_.byteOrder(), state->baseOffset());
    } else {
      mnState_ = *state;
    }
  }
  pState_ = &mnState_;
}

ByteOrder TiffReader::byteOrder() const {
  assert(pState_);
  return pState_->byteOrder();
}

uint32_t TiffReader::baseOffset() const {
  assert(pState_);
  return pState_->baseOffset();
}

void TiffReader::readDataEntryBase(TiffDataEntryBase* object) {
  assert(object != 0);

  readTiffEntry(object);
  TiffFinder finder(object->szTag(), object->szGroup());
  pRoot_->accept(finder);
  auto te = dynamic_cast<TiffEntryBase*>(finder.result());
  if (te && te->pValue()) {
    object->setStrips(te->pValue(), pData_, size_, baseOffset());
  }
}

void TiffReader::visitEntry(TiffEntry* object) {
  readTiffEntry(object);
}

void TiffReader::visitDataEntry(TiffDataEntry* object) {
  readDataEntryBase(object);
}

void TiffReader::visitImageEntry(TiffImageEntry* object) {
  readDataEntryBase(object);
}

void TiffReader::visitSizeEntry(TiffSizeEntry* object) {
  assert(object != 0);

  readTiffEntry(object);
  TiffFinder finder(object->dtTag(), object->dtGroup());
  pRoot_->accept(finder);
  auto te = dynamic_cast<TiffDataEntryBase*>(finder.result());
  if (te && te->pValue()) {
    te->setStrips(object->pValue(), pData_, size_, baseOffset());
  }
}

bool TiffReader::circularReference(const byte* start, IfdId group) {
  DirList::const_iterator pos = dirList_.find(start);
  if (pos != dirList_.end()) {
#ifndef SUPPRESS_WARNINGS
    EXV_ERROR << groupName(group) << " pointer references previously read " << groupName(pos->second)
              << " directory; ignored.\n";
#endif
    return true;
  }
  dirList_[start] = group;
  return false;
}

int TiffReader::nextIdx(IfdId group) {
  return ++idxSeq_[group];
}

void TiffReader::postProcess() {
  setMnState();  // All components to be post-processed must be from the Makernote
  postProc_ = true;
  for (auto&& pos : postList_) {
    pos->accept(*this);
  }
  postProc_ = false;
  setOrigState();
}

void TiffReader::visitDirectory(TiffDirectory* object) {
  assert(object != 0);

  const byte* p = object->start();
  assert(p >= pData_);

  if (circularReference(object->start(), object->group()))
    return;

  if (p + 2 > pLast_) {
#ifndef SUPPRESS_WARNINGS
    EXV_ERROR << "Directory " << groupName(object->group()) << ": IFD exceeds data buffer, cannot read entry count.\n";
#endif
    return;
  }
  const uint16_t n = getUShort(p, byteOrder());
  p += 2;
  // Sanity check with an "unreasonably" large number
  if (n > 256) {
#ifndef SUPPRESS_WARNINGS
    EXV_ERROR << "Directory " << groupName(object->group()) << " with " << n
              << " entries considered invalid; not read.\n";
#endif
    return;
  }
  for (uint16_t i = 0; i < n; ++i) {
    if (p + 12 > pLast_) {
#ifndef SUPPRESS_WARNINGS
      EXV_ERROR << "Directory " << groupName(object->group()) << ": IFD entry " << i
                << " lies outside of the data buffer.\n";
#endif
      return;
    }
    uint16_t tag = getUShort(p, byteOrder());
    TiffComponent::UniquePtr tc = TiffCreator::create(tag, object->group());
    if (tc.get()) {
      tc->setStart(p);
      object->addChild(std::move(tc));
    } else {
#ifndef SUPPRESS_WARNINGS
      EXV_WARNING << "Unable to handle tag " << tag << ".\n";
#endif
    }
    p += 12;
  }

  if (object->hasNext()) {
    if (p + 4 > pLast_) {
#ifndef SUPPRESS_WARNINGS
      EXV_ERROR << "Directory " << groupName(object->group())
                << ": IFD exceeds data buffer, cannot read next pointer.\n";
#endif
      return;
    }
    TiffComponent::UniquePtr tc;
    uint32_t next = getLong(p, byteOrder());
    if (next) {
      tc = TiffCreator::create(Tag::next, object->group());
#ifndef SUPPRESS_WARNINGS
      if (tc.get() == nullptr) {
        EXV_WARNING << "Directory " << groupName(object->group()) << " has an unexpected next pointer; ignored.\n";
      }
#endif
    }
    if (tc.get()) {
      if (baseOffset() + next > size_) {
#ifndef SUPPRESS_WARNINGS
        EXV_ERROR << "Directory " << groupName(object->group()) << ": Next pointer is out of bounds; ignored.\n";
#endif
        return;
      }
      tc->setStart(pData_ + baseOffset() + next);
      object->addNext(std::move(tc));
    }
  }  // object->hasNext()

}  // TiffReader::visitDirectory

void TiffReader::visitSubIfd(TiffSubIfd* object) {
  assert(object != 0);

  readTiffEntry(object);
  if ((object->tiffType() == ttUnsignedLong || object->tiffType() == ttSignedLong || object->tiffType() == ttTiffIfd) &&
      object->count() >= 1) {
    // Todo: Fix hack
    uint32_t maxi = 9;
    if (object->group() == ifd1Id)
      maxi = 1;
    for (uint32_t i = 0; i < object->count(); ++i) {
      uint32_t offset = getLong(object->pData() + 4 * i, byteOrder());
      if (baseOffset() + offset > size_) {
#ifndef SUPPRESS_WARNINGS
        EXV_ERROR << "Directory " << groupName(object->group()) << ", entry 0x" << std::setw(4) << std::setfill('0')
                  << std::hex << object->tag() << " Sub-IFD pointer " << i << " is out of bounds; ignoring it.\n";
#endif
        return;
      }
      if (i >= maxi) {
#ifndef SUPPRESS_WARNINGS
        EXV_WARNING << "Directory " << groupName(object->group()) << ", entry 0x" << std::setw(4) << std::setfill('0')
                    << std::hex << object->tag() << ": Skipping sub-IFDs beyond the first " << i << ".\n";
#endif
        break;
      }
      // If there are multiple dirs, group is incremented for each
      TiffComponent::UniquePtr td(new TiffDirectory(object->tag(), static_cast<IfdId>(object->newGroup_ + i)));
      td->setStart(pData_ + baseOffset() + offset);
      object->addChild(std::move(td));
    }
  }
#ifndef SUPPRESS_WARNINGS
  else {
    EXV_WARNING << "Directory " << groupName(object->group()) << ", entry 0x" << std::setw(4) << std::setfill('0')
                << std::hex << object->tag() << " doesn't look like a sub-IFD.\n";
  }
#endif

}  // TiffReader::visitSubIfd

void TiffReader::visitMnEntry(TiffMnEntry* object) {
  assert(object != 0);

  readTiffEntry(object);
  // Find camera make
  TiffFinder finder(0x010f, ifd0Id);
  pRoot_->accept(finder);
  auto te = dynamic_cast<TiffEntryBase*>(finder.result());
  std::string make;
  if (te && te->pValue()) {
    make = te->pValue()->toString();
    // create concrete makernote, based on make and makernote contents
    object->mn_ =
        TiffMnCreator::create(object->tag(), object->mnGroup_, make, object->pData_, object->size_, byteOrder());
  }
  if (object->mn_)
    object->mn_->setStart(object->pData());

}  // TiffReader::visitMnEntry

void TiffReader::visitIfdMakernote(TiffIfdMakernote* object) {
  assert(object != 0);

  object->setImageByteOrder(byteOrder());  // set the byte order for the image

  if (!object->readHeader(object->start(), static_cast<uint32_t>(pLast_ - object->start()), byteOrder())) {
#ifndef SUPPRESS_WARNINGS
    EXV_ERROR << "Failed to read " << groupName(object->ifd_.group()) << " IFD Makernote header.\n";
#ifdef EXIV2_DEBUG_MESSAGES
    if (static_cast<uint32_t>(pLast_ - object->start()) >= 16) {
      hexdump(std::cerr, object->start(), 16);
    }
#endif  // EXIV2_DEBUG_MESSAGES
#endif  // SUPPRESS_WARNINGS
    setGo(geKnownMakernote, false);
    return;
  }

  object->ifd_.setStart(object->start() + object->ifdOffset());

  // Modify reader for Makernote peculiarities, byte order and offset
  object->mnOffset_ = static_cast<uint32_t>(object->start() - pData_);
  TiffRwState state(object->byteOrder(), object->baseOffset());
  setMnState(&state);

}  // TiffReader::visitIfdMakernote

void TiffReader::visitIfdMakernoteEnd(TiffIfdMakernote* /*object*/) {
  // Reset state (byte order, create function, offset) back to that for the image
  setOrigState();
}  // TiffReader::visitIfdMakernoteEnd

void TiffReader::readTiffEntry(TiffEntryBase* object) {
  assert(object != 0);

  byte* p = object->start();
  assert(p >= pData_);

  if (p + 12 > pLast_) {
#ifndef SUPPRESS_WARNINGS
    EXV_ERROR << "Entry in directory " << groupName(object->group())
              << "requests access to memory beyond the data buffer. "
              << "Skipping entry.\n";
#endif
    return;
  }
  // Component already has tag
  p += 2;
  TiffType tiffType = getUShort(p, byteOrder());
  TypeId typeId = toTypeId(tiffType, object->tag(), object->group());
  long typeSize = TypeInfo::typeSize(typeId);
  if (0 == typeSize) {
#ifndef SUPPRESS_WARNINGS
    EXV_WARNING << "Directory " << groupName(object->group()) << ", entry 0x" << std::setw(4) << std::setfill('0')
                << std::hex << object->tag() << " has unknown Exif (TIFF) type " << std::dec << tiffType
                << "; setting type size 1.\n";
#endif
    typeSize = 1;
  }
  p += 2;
  uint32_t count = getULong(p, byteOrder());
  if (count >= 0x10000000) {
#ifndef SUPPRESS_WARNINGS
    EXV_ERROR << "Directory " << groupName(object->group()) << ", entry 0x" << std::setw(4) << std::setfill('0')
              << std::hex << object->tag() << " has invalid size " << std::dec << count << "*" << typeSize
              << "; skipping entry.\n";
#endif
    return;
  }
  p += 4;
  uint32_t isize = 0;  // size of Exif.Sony1.PreviewImage

  if (count > std::numeric_limits<uint32_t>::max() / typeSize) {
    throw Error(kerArithmeticOverflow);
  }
  uint32_t size = typeSize * count;
  uint32_t offset = getLong(p, byteOrder());
  byte* pData = p;
  if (size > 4 && (baseOffset() + offset >= size_ || static_cast<int32_t>(baseOffset()) + offset <= 0)) {
    // #1143
    if (object->tag() == 0x2001 && std::string(groupName(object->group())) == "Sony1") {
      isize = size;
    } else {
#ifndef SUPPRESS_WARNINGS
      EXV_ERROR << "Offset of directory " << groupName(object->group()) << ", entry 0x" << std::setw(4)
                << std::setfill('0') << std::hex << object->tag() << " is out of bounds: "
                << "Offset = 0x" << std::setw(8) << std::setfill('0') << std::hex << offset
                << "; truncating the entry\n";
#endif
    }
    size = 0;
  }
  if (size > 4) {
    // setting pData to pData_ + baseOffset() + offset can result in pData pointing to invalid memory,
    // as offset can be arbitrarily large
    if ((static_cast<uintptr_t>(baseOffset()) >
         std::numeric_limits<uintptr_t>::max() - static_cast<uintptr_t>(offset)) ||
        (static_cast<uintptr_t>(baseOffset() + offset) >
         std::numeric_limits<uintptr_t>::max() - reinterpret_cast<uintptr_t>(pData_))) {
      throw Error(kerCorruptedMetadata);  // #562 don't throw kerArithmeticOverflow
    }
    if (pData_ + static_cast<uintptr_t>(baseOffset()) + static_cast<uintptr_t>(offset) > pLast_) {
      throw Error(kerCorruptedMetadata);
    }
    pData = const_cast<byte*>(pData_) + baseOffset() + offset;

    // check for size being invalid
    if (size > static_cast<uint32_t>(pLast_ - pData)) {
#ifndef SUPPRESS_WARNINGS
      EXV_ERROR << "Upper boundary of data for "
                << "directory " << groupName(object->group()) << ", entry 0x" << std::setw(4) << std::setfill('0')
                << std::hex << object->tag() << " is out of bounds: "
                << "Offset = 0x" << std::setw(8) << std::setfill('0') << std::hex << offset << ", size = " << std::dec
                << size
                << ", exceeds buffer size by "
                // cast to make MSVC happy
                << static_cast<uint32_t>(pData + size - pLast_) << " Bytes; truncating the entry\n";
#endif
      size = 0;
    }
  }
  Value::UniquePtr v = Value::create(typeId);
  enforce(v.get() != nullptr, kerCorruptedMetadata);
  if (!isize) {
    v->read(pData, size, byteOrder());
  } else {
    // #1143 Write a "hollow" buffer for the preview image
    //       Sadly: we don't know the exact location of the image in the source (it's near offset)
    //       And neither TiffReader nor TiffEntryBase have access to the BasicIo object being processed
    std::vector<byte> buffer(isize);
    v->read(buffer.data(), isize, byteOrder());
  }

  object->setValue(std::move(v));
  object->setData(pData, size);
  object->setOffset(offset);
  object->setIdx(nextIdx(object->group()));

}  // TiffReader::readTiffEntry

void TiffReader::visitBinaryArray(TiffBinaryArray* object) {
  assert(object != 0);

  if (!postProc_) {
    // Defer reading children until after all other components are read, but
    // since state (offset) is not set during post-processing, read entry here
    readTiffEntry(object);
    object->iniOrigDataBuf();
    postList_.push_back(object);
    return;
  }
  // Check duplicates
  TiffFinder finder(object->tag(), object->group());
  pRoot_->accept(finder);
  auto te = dynamic_cast<TiffEntryBase*>(finder.result());
  if (te && te->idx() != object->idx()) {
#ifndef SUPPRESS_WARNINGS
    EXV_WARNING << "Not decoding duplicate binary array tag 0x" << std::setw(4) << std::setfill('0') << std::hex
                << object->tag() << std::dec << ", group " << groupName(object->group()) << ", idx " << object->idx()
                << "\n";
#endif
    object->setDecoded(false);
    return;
  }

  if (object->TiffEntryBase::doSize() == 0)
    return;
  if (!object->initialize(pRoot_))
    return;
  const ArrayCfg* cfg = object->cfg();
  if (cfg == nullptr)
    return;

  const CryptFct cryptFct = cfg->cryptFct_;
  if (cryptFct != nullptr) {
    const byte* pData = object->pData();
    int32_t size = object->TiffEntryBase::doSize();
    DataBuf buf = cryptFct(object->tag(), pData, size, pRoot_);
    if (buf.size_ > 0)
      object->setData(buf);
  }

  const ArrayDef* defs = object->def();
  const ArrayDef* defsEnd = defs + object->defSize();
  const ArrayDef* def = &cfg->elDefaultDef_;
  ArrayDef gap = *def;

  for (uint32_t idx = 0; idx < object->TiffEntryBase::doSize();) {
    if (defs) {
      def = std::find(defs, defsEnd, idx);
      if (def == defsEnd) {
        if (cfg->concat_) {
          // Determine gap-size
          const ArrayDef* xdef = defs;
          for (; xdef != defsEnd && xdef->idx_ <= idx; ++xdef) {
          }
          uint32_t gapSize = 0;
          if (xdef != defsEnd && xdef->idx_ > idx) {
            gapSize = xdef->idx_ - idx;
          } else {
            gapSize = object->TiffEntryBase::doSize() - idx;
          }
          gap.idx_ = idx;
          gap.tiffType_ = cfg->elDefaultDef_.tiffType_;
          gap.count_ = gapSize / cfg->tagStep();
          if (gap.count_ * cfg->tagStep() != gapSize) {
            gap.tiffType_ = ttUndefined;
            gap.count_ = gapSize;
          }
          def = &gap;
        } else {
          def = &cfg->elDefaultDef_;
        }
      }
    }
    idx += object->addElement(idx, *def);  // idx may be different from def->idx_
  }

}  // TiffReader::visitBinaryArray

void TiffReader::visitBinaryElement(TiffBinaryElement* object) {
  byte* pData = object->start();
  uint32_t size = object->TiffEntryBase::doSize();
  ByteOrder bo = object->elByteOrder();
  if (bo == invalidByteOrder)
    bo = byteOrder();
  TypeId typeId = toTypeId(object->elDef()->tiffType_, object->tag(), object->group());
  Value::UniquePtr v = Value::create(typeId);
  enforce(v.get() != nullptr, kerCorruptedMetadata);
  v->read(pData, size, bo);

  object->setValue(std::move(v));
  object->setOffset(0);
  object->setIdx(nextIdx(object->group()));

}  // TiffReader::visitBinaryElement

}  // namespace Internal
}  // namespace Exiv2
