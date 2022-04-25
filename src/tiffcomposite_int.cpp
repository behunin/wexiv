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
#include "tiffcomposite_int.hpp"

#include "config.h"
#include "enforce.hpp"
#include "error.hpp"
#include "makernote_int.hpp"
#include "sonymn_int.hpp"
#include "tiffimage_int.hpp"
#include "tiffvisitor_int.hpp"
#include "value.hpp"

namespace Exiv2 {
namespace Internal {

bool TiffMappingInfo::operator==(const TiffMappingInfo::Key& key) const {
  return (0 == strcmp("*", make_) || 0 == strncmp(make_, key.m_.c_str(), strlen(make_))) &&
         (Tag::all == extendedTag_ || key.e_ == extendedTag_) && key.g_ == group_;
}

IoWrapper::IoWrapper(BasicIo& io, const byte* pHeader, long size, OffsetWriter* pow) :
    io_(io), pHeader_(pHeader), size_(size), wroteHeader_(false), pow_(pow) {
  if (pHeader_ == nullptr || size_ == 0)
    wroteHeader_ = true;
}

TiffComponent::TiffComponent(uint16_t tag, IfdId group) : tag_(tag), group_(group) {
}

TiffEntryBase::TiffEntryBase(uint16_t tag, IfdId group, TiffType tiffType) :
    TiffComponent(tag, group), tiffType_(tiffType) {
}

TiffSubIfd::TiffSubIfd(uint16_t tag, IfdId group, IfdId newGroup) :
    TiffEntryBase(tag, group, ttUnsignedLong), newGroup_(newGroup) {
}

TiffMnEntry::TiffMnEntry(uint16_t tag, IfdId group, IfdId mnGroup) :
    TiffEntryBase(tag, group, ttUndefined), mnGroup_(mnGroup) {
}

TiffIfdMakernote::TiffIfdMakernote(uint16_t tag, IfdId group, IfdId mnGroup, MnHeader* pHeader, bool hasNext) :
    TiffComponent(tag, group), pHeader_(pHeader), ifd_(tag, mnGroup, hasNext), imageByteOrder_(invalidByteOrder) {
}

TiffBinaryArray::TiffBinaryArray(uint16_t tag, IfdId group, const ArrayCfg* arrayCfg, const ArrayDef* arrayDef,
                                 int defSize) :
    TiffEntryBase(tag, group, arrayCfg->elTiffType_), arrayCfg_(arrayCfg), arrayDef_(arrayDef), defSize_(defSize) {
}

TiffBinaryArray::TiffBinaryArray(uint16_t tag, IfdId group, const ArraySet* arraySet, int setSize,
                                 CfgSelFct cfgSelFct) :
    TiffEntryBase(tag, group),  // Todo: Does it make a difference that there is no type?
    cfgSelFct_(cfgSelFct),
    arraySet_(arraySet),
    setSize_(setSize) {
  // We'll figure out the correct cfg later
}

TiffBinaryElement::TiffBinaryElement(uint16_t tag, IfdId group) :
    TiffEntryBase(tag, group), elByteOrder_(invalidByteOrder) {
  elDef_.idx_ = 0;
  elDef_.tiffType_ = ttUndefined;
  elDef_.count_ = 0;
}

TiffDirectory::~TiffDirectory() {
  for (auto&& component : components_) {
    delete component;
  }
  delete pNext_;
}

TiffSubIfd::~TiffSubIfd() {
  for (auto&& ifd : ifds_) {
    delete ifd;
  }
}

TiffEntryBase::~TiffEntryBase() {
  delete pValue_;
}

TiffMnEntry::~TiffMnEntry() {
  delete mn_;
}

TiffIfdMakernote::~TiffIfdMakernote() {
  delete pHeader_;
}

TiffBinaryArray::~TiffBinaryArray() {
  for (auto&& element : elements_) {
    delete element;
  }
}

TiffEntryBase::TiffEntryBase(const TiffEntryBase& rhs) :
    TiffComponent(rhs),
    tiffType_(rhs.tiffType_),
    count_(rhs.count_),
    offset_(rhs.offset_),
    size_(rhs.size_),
    pData_(rhs.pData_),
    idx_(rhs.idx_),
    pValue_(rhs.pValue_ ? rhs.pValue_->clone().release() : nullptr),
    storage_(rhs.storage_) {
}

TiffDirectory::TiffDirectory(const TiffDirectory& rhs) : TiffComponent(rhs), hasNext_(rhs.hasNext_) {
}

TiffSubIfd::TiffSubIfd(const TiffSubIfd& rhs) : TiffEntryBase(rhs), newGroup_(rhs.newGroup_) {
}

TiffBinaryArray::TiffBinaryArray(const TiffBinaryArray& rhs) :
    TiffEntryBase(rhs),
    cfgSelFct_(rhs.cfgSelFct_),
    arraySet_(rhs.arraySet_),
    arrayCfg_(rhs.arrayCfg_),
    arrayDef_(rhs.arrayDef_),
    defSize_(rhs.defSize_),
    setSize_(rhs.setSize_),
    origData_(rhs.origData_),
    origSize_(rhs.origSize_),
    pRoot_(rhs.pRoot_) {
}

TiffComponent::UniquePtr TiffComponent::clone() const {
  return UniquePtr(doClone());
}

TiffEntry* TiffEntry::doClone() const {
  return new TiffEntry(*this);
}

TiffDataEntry* TiffDataEntry::doClone() const {
  return new TiffDataEntry(*this);
}

TiffImageEntry* TiffImageEntry::doClone() const {
  return new TiffImageEntry(*this);
}

TiffSizeEntry* TiffSizeEntry::doClone() const {
  return new TiffSizeEntry(*this);
}

TiffDirectory* TiffDirectory::doClone() const {
  return new TiffDirectory(*this);
}

TiffSubIfd* TiffSubIfd::doClone() const {
  return new TiffSubIfd(*this);
}

TiffBinaryArray* TiffBinaryArray::doClone() const {
  return new TiffBinaryArray(*this);
}

TiffBinaryElement* TiffBinaryElement::doClone() const {
  return new TiffBinaryElement(*this);
}

int TiffComponent::idx() const {
  return 0;
}

int TiffEntryBase::idx() const {
  return idx_;
}

size_t TiffIfdMakernote::ifdOffset() const {
  if (!pHeader_)
    return 0;
  return pHeader_->ifdOffset();
}

ByteOrder TiffIfdMakernote::byteOrder() const {
  assert(imageByteOrder_ != invalidByteOrder);
  if (!pHeader_ || pHeader_->byteOrder() == invalidByteOrder) {
    return imageByteOrder_;
  }
  return pHeader_->byteOrder();
}

uint32_t TiffIfdMakernote::mnOffset() const {
  return mnOffset_;
}

uint32_t TiffIfdMakernote::baseOffset() const {
  if (!pHeader_)
    return 0;
  return pHeader_->baseOffset(mnOffset_);
}

bool TiffIfdMakernote::readHeader(const byte* pData, size_t size, ByteOrder byteOrder) {
  if (!pHeader_)
    return true;
  return pHeader_->read(pData, size, byteOrder);
}

void TiffIfdMakernote::setByteOrder(ByteOrder byteOrder) {
  if (pHeader_)
    pHeader_->setByteOrder(byteOrder);
}

size_t TiffIfdMakernote::sizeHeader() const {
  if (!pHeader_)
    return 0;
  return pHeader_->size();
}

void TiffComponent::accept(TiffVisitor& visitor) {
  if (visitor.go(TiffVisitor::geTraverse))
    doAccept(visitor);  // one for NVI :)
}  // TiffComponent::accept

void TiffEntry::doAccept(TiffVisitor& visitor) {
  visitor.visitEntry(this);
}  // TiffEntry::doAccept

void TiffDataEntry::doAccept(TiffVisitor& visitor) {
  visitor.visitDataEntry(this);
}  // TiffDataEntry::doAccept

void TiffImageEntry::doAccept(TiffVisitor& visitor) {
  visitor.visitImageEntry(this);
}  // TiffImageEntry::doAccept

void TiffSizeEntry::doAccept(TiffVisitor& visitor) {
  visitor.visitSizeEntry(this);
}  // TiffSizeEntry::doAccept

void TiffDirectory::doAccept(TiffVisitor& visitor) {
  visitor.visitDirectory(this);
  for (auto&& component : components_) {
    if (!visitor.go(TiffVisitor::geTraverse))
      break;
    component->accept(visitor);
  }
  if (visitor.go(TiffVisitor::geTraverse))
    visitor.visitDirectoryNext(this);
  if (pNext_)
    pNext_->accept(visitor);
  if (visitor.go(TiffVisitor::geTraverse))
    visitor.visitDirectoryEnd(this);
}  // TiffDirectory::doAccept

void TiffSubIfd::doAccept(TiffVisitor& visitor) {
  visitor.visitSubIfd(this);
  for (auto&& ifd : ifds_) {
    if (!visitor.go(TiffVisitor::geTraverse))
      break;
    ifd->accept(visitor);
  }
}  // TiffSubIfd::doAccept

void TiffMnEntry::doAccept(TiffVisitor& visitor) {
  visitor.visitMnEntry(this);
  if (mn_)
    mn_->accept(visitor);
  if (!visitor.go(TiffVisitor::geKnownMakernote)) {
    delete mn_;
    mn_ = nullptr;
  }

}  // TiffMnEntry::doAccept

void TiffIfdMakernote::doAccept(TiffVisitor& visitor) {
  if (visitor.go(TiffVisitor::geTraverse))
    visitor.visitIfdMakernote(this);
  if (visitor.go(TiffVisitor::geKnownMakernote))
    ifd_.accept(visitor);
  if (visitor.go(TiffVisitor::geKnownMakernote) && visitor.go(TiffVisitor::geTraverse))
    visitor.visitIfdMakernoteEnd(this);
}

void TiffBinaryArray::doAccept(TiffVisitor& visitor) {
  visitor.visitBinaryArray(this);
  for (auto&& element : elements_) {
    if (!visitor.go(TiffVisitor::geTraverse))
      break;
    element->accept(visitor);
  }
  if (visitor.go(TiffVisitor::geTraverse))
    visitor.visitBinaryArrayEnd(this);
}  // TiffBinaryArray::doAccept

void TiffBinaryElement::doAccept(TiffVisitor& visitor) {
  visitor.visitBinaryElement(this);
}  // TiffBinaryElement::doAccept

uint32_t ArrayDef::size(uint16_t tag, IfdId group) const {
  TypeId typeId = toTypeId(tiffType_, tag, group);
  return count_ * TypeInfo::typeSize(typeId);
}

void TiffEntryBase::setData(const std::shared_ptr<DataBuf>& buf) {
  storage_ = buf;
  pData_ = buf->data();
  size_ = buf->size();
}

void TiffEntryBase::setData(byte* pData, size_t size, const std::shared_ptr<DataBuf>& storage) {
  pData_ = pData;
  size_ = size;
  storage_ = storage;
  if (!pData_)
    size_ = 0;
}

TiffMnEntry* TiffMnEntry::doClone() const {
  return nullptr;
}

TiffComponent* TiffSubIfd::doAddChild(TiffComponent::UniquePtr tiffComponent) {
  auto d = dynamic_cast<TiffDirectory*>(tiffComponent.release());
  ifds_.push_back(d);
  return d;
}  // TiffSubIfd::doAddChild

TiffComponent* TiffMnEntry::doAddChild(TiffComponent::UniquePtr tiffComponent) {
  TiffComponent* tc = nullptr;
  if (mn_) {
    tc = mn_->addChild(std::move(tiffComponent));
  }
  return tc;
}  // TiffMnEntry::doAddChild

TiffComponent* TiffMnEntry::doAddNext(TiffComponent::UniquePtr tiffComponent) {
  TiffComponent* tc = nullptr;
  if (mn_) {
    tc = mn_->addNext(std::move(tiffComponent));
  }
  return tc;
}  // TiffMnEntry::doAddNext

TiffComponent* TiffIfdMakernote::doAddNext(TiffComponent::UniquePtr tiffComponent) {
  return ifd_.addNext(std::move(tiffComponent));
}

TiffComponent* TiffIfdMakernote::doAddChild(TiffComponent::UniquePtr tiffComponent) {
  return ifd_.addChild(std::move(tiffComponent));
}

TiffIfdMakernote* TiffIfdMakernote::doClone() const {
  return nullptr;
}

size_t TiffComponent::count() const {
  return doCount();
}

size_t TiffDirectory::doCount() const {
  return components_.size();
}

TiffComponent* TiffDirectory::doAddNext(TiffComponent::UniquePtr tiffComponent) {
  TiffComponent* tc = nullptr;
  if (hasNext_) {
    tc = tiffComponent.release();
    pNext_ = tc;
  }
  return tc;
}  // TiffDirectory::doAddNext

TiffComponent* TiffDirectory::doAddChild(TiffComponent::UniquePtr tiffComponent) {
  TiffComponent* tc = tiffComponent.release();
  components_.push_back(tc);
  return tc;
}  // TiffDirectory::doAddChild

void TiffImageEntry::setStrips(const Value* pSize, const byte* pData, size_t sizeData, uint32_t baseOffset) {
  if (!pValue() || !pSize) {
    EXV_WARNING << "Directory " << groupName(group()) << ", entry 0x" << std::setw(4) << std::setfill('0') << std::hex
                << tag() << ": Size or data offset value not set, ignoring them.\n";
    return;
  }
  if (pValue()->count() != pSize->count()) {
    EXV_WARNING << "Directory " << groupName(group()) << ", entry 0x" << std::setw(4) << std::setfill('0') << std::hex
                << tag() << ": Size and data offset entries have different"
                << " number of components, ignoring them.\n";
    return;
  }
  for (size_t i = 0; i < pValue()->count(); ++i) {
    const auto offset = pValue()->toUint32(i);
    const byte* pStrip = pData + baseOffset + offset;
    const auto size = pSize->toUint32(i);

    if (offset > sizeData || size > sizeData || baseOffset + offset > sizeData - size) {
      EXV_WARNING << "Directory " << groupName(group()) << ", entry 0x" << std::setw(4) << std::setfill('0') << std::hex
                  << tag() << ": Strip " << std::dec << i << " is outside of the data area; ignored.\n";
    } else if (size != 0) {
      strips_.emplace_back(pStrip, size);
    }
  }
}  // TiffImageEntry::setStrips

void TiffDataEntry::setStrips(const Value* pSize, const byte* pData, size_t sizeData, uint32_t baseOffset) {
  if (!pValue() || !pSize) {
    EXV_WARNING << "Directory " << groupName(group()) << ", entry 0x" << std::setw(4) << std::setfill('0') << std::hex
                << tag() << ": Size or data offset value not set, ignoring them.\n";
    return;
  }
  if (pValue()->count() == 0) {
    EXV_WARNING << "Directory " << groupName(group()) << ", entry 0x" << std::setw(4) << std::setfill('0') << std::hex
                << tag() << ": Data offset entry value is empty, ignoring it.\n";
    return;
  }
  if (pValue()->count() != pSize->count()) {
    EXV_WARNING << "Directory " << groupName(group()) << ", entry 0x" << std::setw(4) << std::setfill('0') << std::hex
                << tag() << ": Size and data offset entries have different"
                << " number of components, ignoring them.\n";
    return;
  }
  size_t size = 0;
  for (size_t i = 0; i < pSize->count(); ++i) {
    size += pSize->toUint32(i);
  }
  auto offset = pValue()->toUint32(0);
  // Todo: Remove limitation of JPEG writer: strips must be contiguous
  // Until then we check: last offset + last size - first offset == size?
  if (pValue()->toUint32(pValue()->count() - 1) + pSize->toUint32(pSize->count() - 1) - offset != size) {
    EXV_WARNING << "Directory " << groupName(group()) << ", entry 0x" << std::setw(4) << std::setfill('0') << std::hex
                << tag() << ": Data area is not contiguous, ignoring it.\n";
    return;
  }
  if (offset > sizeData || size > sizeData || baseOffset + offset > sizeData - size) {
    EXV_WARNING << "Directory " << groupName(group()) << ", entry 0x" << std::setw(4) << std::setfill('0') << std::hex
                << tag() << ": Data area exceeds data buffer, ignoring it.\n";
    return;
  }
  pDataArea_ = const_cast<byte*>(pData) + baseOffset + offset;
  sizeDataArea_ = size;
  const_cast<Value*>(pValue())->setDataArea(pDataArea_, sizeDataArea_);
}  // TiffDataEntry::setStrips

size_t TiffEntryBase::doCount() const {
  return count_;
}

size_t TiffMnEntry::doCount() const {
  if (!mn_) {
    return TiffEntryBase::doCount();
  }
  // Count of IFD makernote in tag Exif.Photo.MakerNote is the size of the
  // Makernote in bytes
  return mn_->size();
}

size_t TiffIfdMakernote::doCount() const {
  return ifd_.count();
}  // TiffIfdMakernote::doCount

size_t TiffBinaryArray::doCount() const {
  if (cfg() == nullptr || !decoded())
    return TiffEntryBase::doCount();

  if (elements_.empty())
    return 0;

  TypeId typeId = toTypeId(tiffType(), tag(), group());
  long typeSize = TypeInfo::typeSize(typeId);
  if (0 == typeSize) {
    EXV_WARNING << "Directory " << groupName(group()) << ", entry 0x" << std::setw(4) << std::setfill('0') << std::hex
                << tag() << " has unknown Exif (TIFF) type " << std::dec << tiffType() << "; setting type size 1.\n";
    typeSize = 1;
  }

  return static_cast<size_t>(static_cast<double>(size()) / typeSize + 0.5);
}

size_t TiffBinaryElement::doCount() const {
  return elDef_.count_;
}

size_t TiffComponent::size() const {
  return doSize();
}  // TiffComponent::size

size_t TiffDirectory::doSize() const {
  size_t compCount = count();
  // Size of the directory, without values and additional data
  size_t len = 2 + 12 * compCount + (hasNext_ ? 4 : 0);
  // Size of IFD values and data
  for (auto&& component : components_) {
    size_t sv = component->size();
    if (sv > 4) {
      sv += sv & 1;  // Align value to word boundary
      len += sv;
    }
    size_t sd = component->sizeData();
    sd += sd & 1;  // Align data to word boundary
    len += sd;
  }
  // Size of next-IFD, if any
  size_t sizeNext = 0;
  if (pNext_) {
    sizeNext = pNext_->size();
    len += sizeNext;
  }
  // Reset size of IFD if it has no entries and no or empty next IFD.
  if (compCount == 0 && sizeNext == 0)
    len = 0;
  return len;
}  // TiffDirectory::doSize

void TiffEntryBase::setValue(Value::UniquePtr value) {
  if (value.get() == nullptr)
    return;
  tiffType_ = toTiffType(value->typeId());
  count_ = value->count();
  delete pValue_;
  pValue_ = value.release();
}  // TiffEntryBase::setValue

size_t TiffEntryBase::doSize() const {
  return size_;
}  // TiffEntryBase::doSize

size_t TiffImageEntry::doSize() const {
  return static_cast<uint32_t>(strips_.size()) * 4;
}  // TiffImageEntry::doSize

size_t TiffSubIfd::doSize() const {
  return static_cast<uint32_t>(ifds_.size()) * 4;
}  // TiffSubIfd::doSize

size_t TiffMnEntry::doSize() const {
  if (!mn_) {
    return TiffEntryBase::doSize();
  }
  return mn_->size();
}  // TiffMnEntry::doSize

size_t TiffIfdMakernote::doSize() const {
  return sizeHeader() + ifd_.size();
}  // TiffIfdMakernote::doSize

bool TiffBinaryArray::initialize(TiffComponent* pRoot) {
  if (!cfgSelFct_)
    return true;  // Not a complex array

  int idx = cfgSelFct_(tag(), pData(), TiffEntryBase::doSize(), pRoot);
  if (idx > -1) {
    arrayCfg_ = &arraySet_[idx].cfg_;
    arrayDef_ = arraySet_[idx].def_;
    defSize_ = int(arraySet_[idx].defSize_);
  }
  return idx > -1;
}

void TiffBinaryArray::iniOrigDataBuf() {
  origData_ = const_cast<byte*>(pData());
  origSize_ = TiffEntryBase::doSize();
}

bool TiffBinaryArray::updOrigDataBuf(const byte* pData, size_t size) {
  if (origSize_ != size)
    return false;
  if (origData_ == pData)
    return true;
  memcpy(origData_, pData, origSize_);
  return true;
}

size_t TiffBinaryArray::doSize() const {
  if (cfg() == nullptr || !decoded())
    return TiffEntryBase::doSize();

  if (elements_.empty())
    return 0;

  // Remaining assumptions:
  // - array elements don't "overlap"
  // - no duplicate tags in the array
  size_t idx = 0;
  size_t sz = cfg()->tagStep();
  for (auto&& element : elements_) {
    if (element->tag() > idx) {
      idx = element->tag();
      sz = element->size();
    }
  }
  idx = idx * cfg()->tagStep() + sz;

  if (cfg()->hasFillers_ && def()) {
    const ArrayDef* lastDef = def() + defSize() - 1;
    auto lastTag = static_cast<uint16_t>(lastDef->idx_ / cfg()->tagStep());
    idx = std::max(idx, static_cast<size_t>(lastDef->idx_ + lastDef->size(lastTag, cfg()->group_)));
  }
  return idx;

}  // TiffBinaryArray::doSize

uint32_t TiffBinaryArray::addElement(uint32_t idx, const ArrayDef& def) {
  auto tag = static_cast<uint16_t>(idx / cfg()->tagStep());
  int32_t sz = std::min(def.size(tag, cfg()->group_), static_cast<uint32_t>(TiffEntryBase::doSize()) - idx);
  TiffComponent::UniquePtr tc = TiffCreator::create(tag, cfg()->group_);
  auto tp = dynamic_cast<TiffBinaryElement*>(tc.get());
  // The assertion typically fails if a component is not configured in
  // the TIFF structure table (TiffCreator::tiffTreeStruct_)
  tp->setStart(pData() + idx);
  tp->setData(const_cast<byte*>(pData() + idx), sz, storage());
  tp->setElDef(def);
  tp->setElByteOrder(cfg()->byteOrder_);
  addChild(std::move(tc));
  return sz;
}  // TiffBinaryArray::addElement

size_t TiffBinaryElement::doSize() const {
  if (!pValue())
    return 0;
  return pValue()->size();
}  // TiffBinaryElement::doSize

size_t TiffComponent::sizeData() const {
  return doSizeData();
}  // TiffComponent::sizeData

size_t TiffDirectory::doSizeData() const {
  assert(false);
  return 0;
}  // TiffDirectory::doSizeData

size_t TiffEntryBase::doSizeData() const {
  return 0;
}  // TiffEntryBase::doSizeData

bool TiffBinaryArray::initialize(IfdId group) {
  if (arrayCfg_ != nullptr)
    return true;  // Not a complex array or already initialized

  for (int idx = 0; idx < setSize_; ++idx) {
    if (arraySet_[idx].cfg_.group_ == group) {
      arrayCfg_ = &arraySet_[idx].cfg_;
      arrayDef_ = arraySet_[idx].def_;
      defSize_ = arraySet_[idx].defSize_;
      return true;
    }
  }
  return false;
}

size_t TiffImageEntry::doSizeData() const {
  size_t len = 0;
  // For makernotes, TIFF image data is written to the data area
  if (group() > mnId) {  // Todo: Fix this hack!!
    len = sizeImage();
  }
  return len;
}  // TiffImageEntry::doSizeData

size_t TiffDataEntry::doSizeData() const {
  if (!pValue())
    return 0;
  return pValue()->sizeDataArea();
}  // TiffDataEntry::doSizeData

size_t TiffSubIfd::doSizeData() const {
  size_t len = 0;
  for (auto&& ifd : ifds_) {
    len += ifd->size();
  }
  return len;
}  // TiffSubIfd::doSizeData

size_t TiffIfdMakernote::doSizeData() const {
  return 0;
}  // TiffIfdMakernote::doSizeData

size_t TiffComponent::sizeImage() const {
  return doSizeImage();
}  // TiffComponent::sizeImage

size_t TiffDirectory::doSizeImage() const {
  size_t len = 0;
  for (auto&& component : components_) {
    len += component->sizeImage();
  }
  if (pNext_) {
    len += pNext_->sizeImage();
  }
  return len;
}  // TiffDirectory::doSizeImage

size_t TiffSubIfd::doSizeImage() const {
  size_t len = 0;
  for (auto&& ifd : ifds_) {
    len += ifd->sizeImage();
  }
  return len;
}  // TiffSubIfd::doSizeImage

size_t TiffIfdMakernote::doSizeImage() const {
  return ifd_.sizeImage();
}  // TiffIfdMakernote::doSizeImage

size_t TiffEntryBase::doSizeImage() const {
  return 0;
}  // TiffEntryBase::doSizeImage

size_t TiffImageEntry::doSizeImage() const {
  if (!pValue())
    return 0;
  auto len = pValue()->sizeDataArea();
  if (len == 0) {
    for (auto&& strip : strips_) {
      len += strip.second;
    }
  }
  return len;
}  // TiffImageEntry::doSizeImage

TiffComponent* TiffComponent::addChild(TiffComponent::UniquePtr tiffComponent) {
  return doAddChild(std::move(tiffComponent));
}  // TiffComponent::addChild

TiffComponent* TiffComponent::doAddChild(UniquePtr /*tiffComponent*/) {
  return nullptr;
}  // TiffComponent::doAddChild

TiffComponent* TiffComponent::addNext(TiffComponent::UniquePtr tiffComponent) {
  return doAddNext(std::move(tiffComponent));
}  // TiffComponent::addNext

TiffComponent* TiffComponent::doAddNext(UniquePtr /*tiffComponent*/) {
  return nullptr;
}  // TiffComponent::doAddNext

static const TagInfo* findTagInfo(uint16_t tag, IfdId group) {
  const TagInfo* result = nullptr;
  const TagInfo* tags = group == exifId ? Internal::exifTagList() : group == gpsId ? Internal::gpsTagList() : nullptr;
  if (tags) {
    for (size_t idx = 0; result == nullptr && tags[idx].tag_ != 0xffff; ++idx) {
      if (tags[idx].tag_ == tag) {
        result = tags + idx;
      }
    }
  }
  return result;
}

// *************************************************************************
// free functions
TypeId toTypeId(TiffType tiffType, uint16_t tag, IfdId group) {
  auto ti = TypeId(tiffType);
  // On the fly type conversion for Exif.Photo.UserComment, Exif.GPSProcessingMethod, GPSAreaInformation
  if (const TagInfo* pTag = ti == undefined ? findTagInfo(tag, group) : nullptr) {
    if (pTag->typeId_ == comment) {
      ti = comment;
    }
  }
  // http://dev.exiv2.org/boards/3/topics/1337 change unsignedByte to signedByte
  // Exif.NikonAFT.AFFineTuneAdj || Exif.Pentax.Temperature
  if (ti == Exiv2::unsignedByte) {
    if ((tag == 0x0002 && group == nikonAFTId) || (tag == 0x0047 && group == pentaxId)) {
      ti = Exiv2::signedByte;
    }
  }
  return ti;
}

TiffType toTiffType(TypeId typeId) {
  if (static_cast<uint32_t>(typeId) > 0xffff) {
    EXV_ERROR << "'" << TypeInfo::typeName(typeId) << "' is not a valid Exif (TIFF) type; using type '"
              << TypeInfo::typeName(undefined) << "'.";
    return undefined;
  }
  return static_cast<uint16_t>(typeId);
}

bool cmpTagLt(TiffComponent const* lhs, TiffComponent const* rhs) {
  if (lhs->tag() != rhs->tag())
    return lhs->tag() < rhs->tag();
  return lhs->idx() < rhs->idx();
}

bool cmpGroupLt(TiffComponent const* lhs, TiffComponent const* rhs) {
  return lhs->group() < rhs->group();
}

TiffComponent::UniquePtr newTiffEntry(uint16_t tag, IfdId group) {
  return std::make_unique<TiffEntry>(tag, group);
}

TiffComponent::UniquePtr newTiffMnEntry(uint16_t tag, IfdId group) {
  return std::make_unique<TiffMnEntry>(tag, group, mnId);
}

TiffComponent::UniquePtr newTiffBinaryElement(uint16_t tag, IfdId group) {
  return std::make_unique<TiffBinaryElement>(tag, group);
}

}  // namespace Internal
}  // namespace Exiv2
