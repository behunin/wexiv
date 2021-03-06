// SPDX-License-Identifier: GPL-2.0-or-later

// *****************************************************************************
#include "iptc.hpp"

#include "datasets.hpp"
#include "enforce.hpp"
#include "error.hpp"
#include "image_int.hpp"
#include "types.hpp"
#include "value.hpp"

// + standard includes
#include <algorithm>
#include <iostream>

// *****************************************************************************
namespace {
/*!
  @brief Read a single dataset payload and create a new metadata entry.

  @param iptcData IPTC metadata container to add the dataset to
  @param dataSet  DataSet number
  @param record   Record Id
  @param data     Pointer to the first byte of dataset payload
  @param sizeData Length in bytes of dataset payload
  @return 0 if successful.
*/
int readData(emscripten::val& iptcData, uint16_t dataSet, uint16_t record, const Exiv2::byte* data, uint32_t sizeData);

//! Unary predicate that matches an Iptcdatum with given record and dataset
class FindIptcdatum {
 public:
  //! Constructor, initializes the object with the record and dataset id
  FindIptcdatum(uint16_t dataset, uint16_t record) : dataset_(dataset), record_(record) {
  }
  /*!
    @brief Returns true if the record and dataset id of the argument
          Iptcdatum is equal to that of the object.
  */
  bool operator()(const Exiv2::Iptcdatum& iptcdatum) const {
    return dataset_ == iptcdatum.tag() && record_ == iptcdatum.record();
  }

 private:
  // DATA
  uint16_t dataset_;
  uint16_t record_;

};  // class FindIptcdatum
}  // namespace

// *****************************************************************************
namespace Exiv2 {

Iptcdatum::Iptcdatum(const IptcKey& key, const Value* pValue) : key_(key.clone()) {
  if (pValue)
    value_ = pValue->clone();
}

Iptcdatum::Iptcdatum(const Iptcdatum& rhs) : Metadatum(rhs) {
  if (rhs.key_.get() != nullptr)
    key_ = rhs.key_->clone();  // deep copy
  if (rhs.value_.get() != nullptr)
    value_ = rhs.value_->clone();  // deep copy
}

long Iptcdatum::copy(byte* buf, ByteOrder byteOrder) const {
  return value_.get() == nullptr ? 0 : value_->copy(buf, byteOrder);
}

std::ostream& Iptcdatum::write(std::ostream& os, const ExifData*) const {
  return os << value();
}

std::string Iptcdatum::key() const {
  return key_.get() == nullptr ? "" : key_->key();
}

std::string Iptcdatum::recordName() const {
  return key_.get() == nullptr ? "" : key_->recordName();
}

uint16_t Iptcdatum::record() const {
  return key_.get() == nullptr ? 0 : key_->record();
}

const char* Iptcdatum::familyName() const {
  return key_.get() == nullptr ? "" : key_->familyName();
}

std::string Iptcdatum::groupName() const {
  return key_.get() == nullptr ? "" : key_->groupName();
}

std::string Iptcdatum::tagName() const {
  return key_.get() == nullptr ? "" : key_->tagName();
}

std::string Iptcdatum::tagLabel() const {
  return key_.get() == nullptr ? "" : key_->tagLabel();
}

uint16_t Iptcdatum::tag() const {
  return key_.get() == nullptr ? 0 : key_->tag();
}

TypeId Iptcdatum::typeId() const {
  return value_.get() == nullptr ? invalidTypeId : value_->typeId();
}

const char* Iptcdatum::typeName() const {
  return TypeInfo::typeName(typeId());
}

size_t Iptcdatum::typeSize() const {
  return TypeInfo::typeSize(typeId());
}

size_t Iptcdatum::count() const {
  return value_.get() == nullptr ? 0 : value_->count();
}

size_t Iptcdatum::size() const {
  return value_.get() == nullptr ? 0 : value_->size();
}

std::string Iptcdatum::toString() const {
  return value_.get() == nullptr ? "" : value_->toString();
}

std::string Iptcdatum::toString(long n) const {
  return value_.get() == nullptr ? "" : value_->toString(n);
}

int64_t Iptcdatum::toInt64(size_t n) const {
  return value_ ? value_->toInt64(n) : -1;
}

float Iptcdatum::toFloat(long n) const {
  return value_.get() == nullptr ? -1 : value_->toFloat(n);
}

Rational Iptcdatum::toRational(long n) const {
  return value_.get() == nullptr ? Rational(-1, 1) : value_->toRational(n);
}

Value::UniquePtr Iptcdatum::getValue() const {
  return value_.get() == nullptr ? nullptr : value_->clone();
}

const Value& Iptcdatum::value() const {
  if (value_.get() == nullptr)
    throw Error(kerValueNotSet);
  return *value_;
}

Iptcdatum& Iptcdatum::operator=(const Iptcdatum& rhs) {
  if (this == &rhs)
    return *this;
  Metadatum::operator=(rhs);

  key_.reset();
  if (rhs.key_.get() != nullptr)
    key_ = rhs.key_->clone();  // deep copy

  value_.reset();
  if (rhs.value_.get() != nullptr)
    value_ = rhs.value_->clone();  // deep copy

  return *this;
}  // Iptcdatum::operator=

Iptcdatum& Iptcdatum::operator=(const uint16_t& value) {
  UShortValue::UniquePtr v(new UShortValue);
  v->value_.push_back(value);
  value_ = std::move(v);
  return *this;
}

Iptcdatum& Iptcdatum::operator=(const std::string& value) {
  setValue(value);
  return *this;
}

Iptcdatum& Iptcdatum::operator=(const Value& value) {
  setValue(&value);
  return *this;
}

void Iptcdatum::setValue(const Value* pValue) {
  value_.reset();
  if (pValue)
    value_ = pValue->clone();
}

int Iptcdatum::setValue(const std::string& value) {
  if (value_.get() == nullptr) {
    TypeId type = IptcDataSets::dataSetType(tag(), record());
    value_ = Value::create(type);
  }
  return value_->read(value);
}

Iptcdatum& IptcData::operator[](const std::string& key) {
  IptcKey iptcKey(key);
  auto pos = findKey(iptcKey);
  if (pos == end()) {
    iptcMetadata_.push_back(Iptcdatum(iptcKey));
    return iptcMetadata_.back();
  }
  return *pos;
}

long IptcData::size() const {
  long newSize = 0;
  for (auto&& iptc : iptcMetadata_) {
    // marker, record Id, dataset num, first 2 bytes of size
    newSize += 5;
    long dataSize = iptc.size();
    newSize += dataSize;
    if (dataSize > 32767) {
      // extended dataset (we always use 4 bytes)
      newSize += 4;
    }
  }
  return newSize;
}  // IptcData::size

int IptcData::add(const IptcKey& key, Value* value) {
  return add(Iptcdatum(key, value));
}

int IptcData::add(const Iptcdatum& iptcDatum) {
  if (!IptcDataSets::dataSetRepeatable(iptcDatum.tag(), iptcDatum.record()) &&
      findId(iptcDatum.tag(), iptcDatum.record()) != end()) {
    return 6;
  }
  // allow duplicates
  iptcMetadata_.push_back(iptcDatum);
  return 0;
}

IptcData::const_iterator IptcData::findKey(const IptcKey& key) const {
  return std::find_if(iptcMetadata_.begin(), iptcMetadata_.end(), FindIptcdatum(key.tag(), key.record()));
}

IptcData::iterator IptcData::findKey(const IptcKey& key) {
  return std::find_if(iptcMetadata_.begin(), iptcMetadata_.end(), FindIptcdatum(key.tag(), key.record()));
}

IptcData::const_iterator IptcData::findId(uint16_t dataset, uint16_t record) const {
  return std::find_if(iptcMetadata_.begin(), iptcMetadata_.end(), FindIptcdatum(dataset, record));
}

IptcData::iterator IptcData::findId(uint16_t dataset, uint16_t record) {
  return std::find_if(iptcMetadata_.begin(), iptcMetadata_.end(), FindIptcdatum(dataset, record));
}

void IptcData::sortByKey() {
  std::sort(iptcMetadata_.begin(), iptcMetadata_.end(), cmpMetadataByKey);
}

void IptcData::sortByTag() {
  std::sort(iptcMetadata_.begin(), iptcMetadata_.end(), cmpMetadataByTag);
}

IptcData::iterator IptcData::erase(IptcData::iterator pos) {
  return iptcMetadata_.erase(pos);
}

const char* IptcData::detectCharset() const {
  auto pos = findKey(IptcKey("Iptc.Envelope.CharacterSet"));
  if (pos != end()) {
    const std::string value = pos->toString();
    if (pos->value().ok()) {
      if (value == "\033%G")
        return "UTF-8";
      // other values are probably not practically relevant
    }
  }

  bool ascii = true;
  bool utf8 = true;

  for (pos = begin(); pos != end(); ++pos) {
    std::string value = pos->toString();
    if (pos->value().ok()) {
      int seqCount = 0;
      for (auto&& c : value) {
        if (seqCount) {
          if ((c & 0xc0) != 0x80) {
            utf8 = false;
            break;
          }
          --seqCount;
        } else {
          if (c & 0x80)
            ascii = false;
          else
            continue;  // ascii character

          if ((c & 0xe0) == 0xc0)
            seqCount = 1;
          else if ((c & 0xf0) == 0xe0)
            seqCount = 2;
          else if ((c & 0xf8) == 0xf0)
            seqCount = 3;
          else if ((c & 0xfc) == 0xf8)
            seqCount = 4;
          else if ((c & 0xfe) == 0xfc)
            seqCount = 5;
          else {
            utf8 = false;
            break;
          }
        }
      }
      if (seqCount)
        utf8 = false;  // unterminated seq
      if (!utf8)
        break;
    }
  }

  if (ascii)
    return "ASCII";
  if (utf8)
    return "UTF-8";
  return nullptr;
}

const byte IptcParser::marker_ = 0x1C;  // Dataset marker

int IptcParser::decode(emscripten::val& iptcData, const byte* pData, size_t size) {
#ifdef EXIV2_DEBUG_MESSAGES
  std::cerr << "IptcParser::decode, size = " << size << "\n";
#endif
  const byte* pRead = pData;
  const byte* const pEnd = pData + size;

  uint16_t record = 0;
  uint16_t dataSet = 0;
  uint32_t sizeData = 0;
  byte extTest = 0;

  while (6 <= static_cast<size_t>(pEnd - pRead)) {
    // First byte should be a marker. If it isn't, scan forward and skip
    // the chunk bytes present in some images. This deviates from the
    // standard, which advises to treat such cases as errors.
    if (*pRead++ != marker_)
      continue;
    record = *pRead++;
    dataSet = *pRead++;

    extTest = *pRead;
    if (extTest & 0x80) {
      // extended dataset
      uint16_t sizeOfSize = (getUShort(pRead, bigEndian) & 0x7FFF);
      if (sizeOfSize > 4)
        return 5;
      pRead += 2;
      if (sizeOfSize > static_cast<size_t>(pEnd - pRead))
        return 6;
      sizeData = 0;
      for (; sizeOfSize > 0; --sizeOfSize) {
        sizeData |= *pRead++ << (8 * (sizeOfSize - 1));
      }
    } else {
      // standard dataset
      sizeData = getUShort(pRead, bigEndian);
      pRead += 2;
    }
    if (sizeData <= static_cast<size_t>(pEnd - pRead)) {
      int rc = 0;
      if ((rc = readData(iptcData, dataSet, record, pRead, sizeData)) != 0) {
        EXV_WARNING << "Failed to read IPTC dataset " << IptcKey(dataSet, record).key() << " (rc = " << rc
                    << "); skipped.";
      }
    } else {
      EXV_WARNING << "IPTC dataset " << IptcKey(dataSet, record).key() << " has invalid size " << sizeData
                  << "; skipped.";
      return 7;
    }
    pRead += sizeData;
  }

  return 0;
}  // IptcParser::decode

/*!
  @brief Compare two iptc items by record. Return true if the record of
          lhs is less than that of rhs.

  This is a helper function for IptcParser::encode().
*/
bool cmpIptcdataByRecord(const Iptcdatum& lhs, const Iptcdatum& rhs) {
  return lhs.record() < rhs.record();
}

}  // namespace Exiv2

// *****************************************************************************
namespace {

int readData(emscripten::val& iptcData, uint16_t dataSet, uint16_t record, const Exiv2::byte* data, uint32_t sizeData) {
  Exiv2::Value::UniquePtr value;
  Exiv2::TypeId type = Exiv2::IptcDataSets::dataSetType(dataSet, record);
  value = Exiv2::Value::create(type);
  int rc = value->read(data, sizeData, Exiv2::bigEndian);
  if (0 == rc) {
    Exiv2::IptcKey key(dataSet, record);
    iptcData.set(key.key(), value->toString());
  } else if (1 == rc) {
    // If the first attempt failed, try with a string value
    value = Exiv2::Value::create(Exiv2::string);
    rc = value->read(data, sizeData, Exiv2::bigEndian);
    if (0 == rc) {
      Exiv2::IptcKey key(dataSet, record);
      iptcData.set(key.key(), value->toString());
    }
  }
  return rc;
}

}  // namespace
