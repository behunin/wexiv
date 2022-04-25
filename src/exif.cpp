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
/*
  File:      exif.cpp
  Author(s): Andreas Huggel (ahu) <ahuggel@gmx.net>
  History:   26-Jan-04, ahu: created
             11-Feb-04, ahu: isolated as a component
*/
// *****************************************************************************
#include "exif.hpp"

#include "basicio.hpp"
#include "config.h"
#include "error.hpp"
#include "metadatum.hpp"
#include "safe_op.hpp"
#include "tags.hpp"
#include "tags_int.hpp"
#include "tiffcomposite_int.hpp"  // for Tag::root
#include "tiffimage.hpp"
#include "tiffimage_int.hpp"
#include "types.hpp"
#include "value.hpp"

// + standard includes
#include <algorithm>
#include <array>
#include <cstdio>
#include <utility>

// *****************************************************************************
namespace {

//! Unary predicate that matches a Exifdatum with a given key
class FindExifdatumByKey {
 public:
  //! Constructor, initializes the object with the key to look for
  explicit FindExifdatumByKey(const std::string& key) : key_(key) {
  }
  /*!
    @brief Returns true if the key of \em exifdatum is equal
            to that of the object.
  */
  bool operator()(const Exiv2::Exifdatum& exifdatum) const {
    return key_ == exifdatum.key();
  }

 private:
  const std::string& key_;

};  // class FindExifdatumByKey

/*!
  @brief Exif %Thumbnail image. This abstract base class provides the
          interface for the thumbnail image that is optionally embedded in
          the Exif data. This class is used internally by ExifData, it is
          probably not useful for a client as a standalone class.  Instead,
          use an instance of ExifData to access the Exif thumbnail image.
*/
class Thumbnail {
 public:
  //! Shortcut for a %Thumbnail auto pointer.
  using UniquePtr = std::unique_ptr<Thumbnail>;

  //! @name Creators
  //@{
  //! Virtual destructor
  virtual ~Thumbnail() = default;
  //@}

  //! Factory function to create a thumbnail for the Exif metadata provided.
  static UniquePtr create(const Exiv2::ExifData& exifData);

  //! @name Accessors
  //@{
  /*!
    @brief Return the thumbnail image in a %DataBuf. The caller owns the
            data buffer and %DataBuf ensures that it will be deleted.
  */
  virtual Exiv2::DataBuf copy(const Exiv2::ExifData& exifData) const = 0;
  /*!
    @brief Return the MIME type of the thumbnail ("image/tiff" or
            "image/jpeg").
  */
  virtual const char* mimeType() const = 0;
  /*!
    @brief Return the file extension for the format of the thumbnail
            (".tif", ".jpg").
  */
  virtual const char* extension() const = 0;
  //@}

};  // class Thumbnail

//! Exif thumbnail image in TIFF format
class TiffThumbnail : public Thumbnail {
 public:
  //! Shortcut for a %TiffThumbnail auto pointer.
  using UniquePtr = std::unique_ptr<TiffThumbnail>;

  //! @name Manipulators
  //@{
  //! Assignment operator.
  TiffThumbnail& operator=(const TiffThumbnail& rhs);
  //@}

  //! @name Accessors
  //@{
  Exiv2::DataBuf copy(const Exiv2::ExifData& exifData) const override;
  const char* mimeType() const override;
  const char* extension() const override;

};  // class TiffThumbnail

//! Exif thumbnail image in JPEG format
class JpegThumbnail : public Thumbnail {
 public:
  //! Shortcut for a %JpegThumbnail auto pointer.
  using UniquePtr = std::unique_ptr<JpegThumbnail>;

  //! @name Manipulators
  //@{
  //! Assignment operator.
  JpegThumbnail& operator=(const JpegThumbnail& rhs);
  //@}

  //! @name Accessors
  //@{
  Exiv2::DataBuf copy(const Exiv2::ExifData& exifData) const override;
  const char* mimeType() const override;
  const char* extension() const override;
  //@}

};  // class JpegThumbnail

//! Helper function to sum all components of the value of a metadatum
int64_t sumToLong(const Exiv2::Exifdatum& md);

//! Helper function to delete all tags of a specific IFD from the metadata.
void eraseIfd(Exiv2::ExifData& ed, Exiv2::Internal::IfdId ifdId);

}  // namespace

namespace Exiv2 {

using namespace Internal;

/*!
  @brief Set the value of \em exifDatum to \em value. If the object already
          has a value, it is replaced. Otherwise a new ValueType\<T\> value
          is created and set to \em value.

  This is a helper function, called from Exifdatum members. It is meant to
  be used with T = (u)int16_t, (u)int32_t or (U)Rational. Do not use directly.
*/
template <typename T>
Exiv2::Exifdatum& setValue(Exiv2::Exifdatum& exifDatum, const T& value) {
  auto v = std::unique_ptr<Exiv2::ValueType<T>>(new Exiv2::ValueType<T>);
  v->value_.push_back(value);
  exifDatum.value_ = std::move(v);
  return exifDatum;
}

Exifdatum::Exifdatum(const ExifKey& key, const Value* pValue) : key_(key.clone()) {
  if (pValue)
    value_ = pValue->clone();
}

Exifdatum::Exifdatum(const Exifdatum& rhs) : Metadatum(rhs) {
  if (rhs.key_.get() != nullptr)
    key_ = rhs.key_->clone();  // deep copy
  if (rhs.value_.get() != nullptr)
    value_ = rhs.value_->clone();  // deep copy
}

std::ostream& Exifdatum::write(std::ostream& os, const ExifData* pMetadata) const {
  if (value().count() == 0)
    return os;

  PrintFct fct = printValue;
  const TagInfo* ti = Internal::tagInfo(tag(), static_cast<IfdId>(ifdId()));
  // be careful with comments (User.Photo.UserComment, GPSAreaInfo etc).
  if (ti) {
    fct = ti->printFct_;
    if (ti->typeId_ == comment) {
      os << value().toString();
      fct = nullptr;
    }
  }
  if (fct) {
    // https://github.com/Exiv2/exiv2/issues/1706
    // Sometimes the type of the value doesn't match what the
    // print function expects. (The expected types are stored
    // in the TagInfo tables, but they are not enforced when the
    // metadata is parsed.) These type mismatches can sometimes
    // cause a std::out_of_range exception to be thrown.
    try {
      fct(os, value(), pMetadata);
    } catch (std::out_of_range&) {
      os << "Bad value";
#ifdef EXIV2_DEBUG_MESSAGES
      std::cerr << "Caught std::out_of_range exception in Exifdatum::write().\n";
#endif
    }
  }
  return os;
}

const Value& Exifdatum::value() const {
  if (value_.get() == nullptr)
    throw Error(kerValueNotSet);
  return *value_;
}

Exifdatum& Exifdatum::operator=(const Exifdatum& rhs) {
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
}  // Exifdatum::operator=

Exifdatum& Exifdatum::operator=(const std::string& value) {
  setValue(value);
  return *this;
}

Exifdatum& Exifdatum::operator=(const uint16_t& value) {
  return Exiv2::setValue(*this, value);
}

Exifdatum& Exifdatum::operator=(const uint32_t& value) {
  return Exiv2::setValue(*this, value);
}

Exifdatum& Exifdatum::operator=(const URational& value) {
  return Exiv2::setValue(*this, value);
}

Exifdatum& Exifdatum::operator=(const int16_t& value) {
  return Exiv2::setValue(*this, value);
}

Exifdatum& Exifdatum::operator=(const int32_t& value) {
  return Exiv2::setValue(*this, value);
}

Exifdatum& Exifdatum::operator=(const Rational& value) {
  return Exiv2::setValue(*this, value);
}

Exifdatum& Exifdatum::operator=(const Value& value) {
  setValue(&value);
  return *this;
}

void Exifdatum::setValue(const Value* pValue) {
  value_.reset();
  if (pValue)
    value_ = pValue->clone();
}

int Exifdatum::setValue(const std::string& value) {
  if (value_.get() == nullptr) {
    TypeId type = key_->defaultTypeId();
    value_ = Value::create(type);
  }
  return value_->read(value);
}

int Exifdatum::setDataArea(const byte* buf, long len) {
  return value_.get() == nullptr ? -1 : value_->setDataArea(buf, len);
}

std::string Exifdatum::key() const {
  return key_.get() == nullptr ? "" : key_->key();
}

const char* Exifdatum::familyName() const {
  return key_.get() == nullptr ? "" : key_->familyName();
}

std::string Exifdatum::groupName() const {
  return key_.get() == nullptr ? "" : key_->groupName();
}

std::string Exifdatum::tagName() const {
  return key_.get() == nullptr ? "" : key_->tagName();
}

std::string Exifdatum::tagLabel() const {
  return key_.get() == nullptr ? "" : key_->tagLabel();
}

uint16_t Exifdatum::tag() const {
  return key_.get() == nullptr ? 0xffff : key_->tag();
}

int Exifdatum::ifdId() const {
  return key_.get() == nullptr ? ifdIdNotSet : key_->ifdId();
}

const char* Exifdatum::ifdName() const {
  return key_.get() == nullptr ? "" : Internal::ifdName(static_cast<Internal::IfdId>(key_->ifdId()));
}

int Exifdatum::idx() const {
  return key_.get() == nullptr ? 0 : key_->idx();
}

long Exifdatum::copy(byte* buf, ByteOrder byteOrder) const {
  return value_.get() == nullptr ? 0 : value_->copy(buf, byteOrder);
}

TypeId Exifdatum::typeId() const {
  return value_.get() == nullptr ? invalidTypeId : value_->typeId();
}

const char* Exifdatum::typeName() const {
  return TypeInfo::typeName(typeId());
}

size_t Exifdatum::typeSize() const {
  return TypeInfo::typeSize(typeId());
}

size_t Exifdatum::count() const {
  return value_.get() == nullptr ? 0 : value_->count();
}

size_t Exifdatum::size() const {
  return value_.get() == nullptr ? 0 : value_->size();
}

std::string Exifdatum::toString() const {
  return value_.get() == nullptr ? "" : value_->toString();
}

std::string Exifdatum::toString(long n) const {
  return value_.get() == nullptr ? "" : value_->toString(n);
}

int64_t Exifdatum::toInt64(size_t n) const {
  return value_ ? value_->toInt64(n) : -1;
}

float Exifdatum::toFloat(long n) const {
  return value_.get() == nullptr ? -1 : value_->toFloat(n);
}

Rational Exifdatum::toRational(long n) const {
  return value_.get() == nullptr ? Rational(-1, 1) : value_->toRational(n);
}

Value::UniquePtr Exifdatum::getValue() const {
  return value_.get() == nullptr ? nullptr : value_->clone();
}

long Exifdatum::sizeDataArea() const {
  return value_.get() == nullptr ? 0 : value_->sizeDataArea();
}

DataBuf Exifdatum::dataArea() const {
  return value_.get() == nullptr ? DataBuf(nullptr, 0) : value_->dataArea();
}

Exifdatum& ExifData::operator[](const std::string& key) {
  ExifKey exifKey(key);
  auto pos = findKey(exifKey);
  if (pos == end()) {
    exifMetadata_.push_back(Exifdatum(exifKey));
    return exifMetadata_.back();
  }
  return *pos;
}

void ExifData::add(const ExifKey& key, const Value* pValue) {
  add(Exifdatum(key, pValue));
}

void ExifData::add(const Exifdatum& exifdatum) {
  // allow duplicates
  exifMetadata_.push_back(exifdatum);
}

ExifData::const_iterator ExifData::findKey(const ExifKey& key) const {
  return std::find_if(exifMetadata_.begin(), exifMetadata_.end(), FindExifdatumByKey(key.key()));
}

ExifData::iterator ExifData::findKey(const ExifKey& key) {
  return std::find_if(exifMetadata_.begin(), exifMetadata_.end(), FindExifdatumByKey(key.key()));
}

void ExifData::clear() {
  exifMetadata_.clear();
}

void ExifData::sortByKey() {
  exifMetadata_.sort(cmpMetadataByKey);
}

void ExifData::sortByTag() {
  exifMetadata_.sort(cmpMetadataByTag);
}

ByteOrder ExifParser::decode(emscripten::val& exifData, const byte* pData, size_t size) {
  auto iptcData = emscripten::val::object();
  auto xmpData = emscripten::val::object();
  ByteOrder bo = TiffParser::decode(exifData, iptcData, xmpData, pData, size);
  return bo;
}  // ExifParser::decode
//! @endcond

}  // namespace Exiv2

namespace {

//! @cond IGNORE
const char* TiffThumbnail::mimeType() const {
  return "image/tiff";
}

const char* TiffThumbnail::extension() const {
  return ".tif";
}

const char* JpegThumbnail::mimeType() const {
  return "image/jpeg";
}

const char* JpegThumbnail::extension() const {
  return ".jpg";
}

int64_t sumToLong(const Exiv2::Exifdatum& md) {
  int64_t sum = 0;
  for (size_t i = 0; i < md.count(); ++i) {
    sum = Safe::add(sum, md.toInt64(i));
  }
  return sum;
}
//! @endcond
}  // namespace
