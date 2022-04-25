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
/*!
  @file    exif.hpp
  @brief   Encoding and decoding of Exif data
  @author  Andreas Huggel (ahu)
           <a href="mailto:ahuggel@gmx.net">ahuggel@gmx.net</a>
  @date    09-Jan-04, ahu: created
 */
#ifndef EXIF_HPP_
#define EXIF_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

// included header files
#include "metadatum.hpp"
#include "tags.hpp"

#include <emscripten/val.h>

// + standard includes
#include <list>

// *****************************************************************************
// namespace extensions
/*!
  @brief Provides classes and functions to encode and decode Exif and Iptc data.
         The <b>libexiv2</b> API consists of the objects of this namespace.
*/
namespace Exiv2 {

// *****************************************************************************
// class declarations
class ExifData;

// *****************************************************************************
// class definitions

/*!
  @brief An Exif metadatum, consisting of an ExifKey and a Value and
        methods to manipulate these.
*/
class EXIV2API Exifdatum : public Metadatum {
  template <typename T>
  friend Exifdatum& setValue(Exifdatum&, const T&);

 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor for new tags created by an application. The
            %Exifdatum is created from a \em key / value pair. %Exifdatum copies
            (clones) the \em key and value if one is provided. Alternatively,
            a program can create an 'empty' %Exifdatum with only a key
            and set the value using setValue().

    @param key %ExifKey.
    @param pValue Pointer to an %Exifdatum value.
    @throw Error if the key cannot be parsed and converted.
  */
  explicit Exifdatum(const ExifKey& key, const Value* pValue = nullptr);
  //! Copy constructor
  Exifdatum(const Exifdatum& rhs);
  //! Destructor
  ~Exifdatum() override = default;
  //@}

  //! @name Manipulators
  //@{
  //! Assignment operator
  Exifdatum& operator=(const Exifdatum& rhs);
  /*!
    @brief Assign \em value to the %Exifdatum. The type of the new Value
            is set to UShortValue.
  */
  Exifdatum& operator=(const uint16_t& value);
  /*!
    @brief Assign \em value to the %Exifdatum. The type of the new Value
            is set to ULongValue.
  */
  Exifdatum& operator=(const uint32_t& value);
  /*!
    @brief Assign \em value to the %Exifdatum. The type of the new Value
            is set to URationalValue.
  */
  Exifdatum& operator=(const URational& value);
  /*!
    @brief Assign \em value to the %Exifdatum. The type of the new Value
            is set to ShortValue.
  */
  Exifdatum& operator=(const int16_t& value);
  /*!
    @brief Assign \em value to the %Exifdatum. The type of the new Value
            is set to LongValue.
  */
  Exifdatum& operator=(const int32_t& value);
  /*!
    @brief Assign \em value to the %Exifdatum. The type of the new Value
            is set to RationalValue.
  */
  Exifdatum& operator=(const Rational& value);
  /*!
    @brief Assign \em value to the %Exifdatum.
            Calls setValue(const std::string&).
  */
  Exifdatum& operator=(const std::string& value);
  /*!
    @brief Assign \em value to the %Exifdatum.
            Calls setValue(const Value*).
  */
  Exifdatum& operator=(const Value& value);
  void setValue(const Value* pValue) override;
  /*!
    @brief Set the value to the string \em value.  Uses Value::read(const
            std::string&).  If the %Exifdatum does not have a Value yet,
            then a %Value of the correct type for this %Exifdatum is
            created. An AsciiValue is created for unknown tags. Return
            0 if the value was read successfully.
  */
  int setValue(const std::string& value) override;
  /*!
    @brief Set the data area by copying (cloning) the buffer pointed to
            by \em buf.

    Values may have a data area, which can contain additional
    information besides the actual value. This method is used to set such
    a data area.

    @param buf Pointer to the source data area
    @param len Size of the data area
    @return Return -1 if the %Exifdatum does not have a value yet or the
            value has no data area, else 0.
  */
  int setDataArea(const byte* buf, long len);
  //@}

  //! @name Accessors
  //@{
  //! Return the key of the %Exifdatum.
  std::string key() const override;
  const char* familyName() const override;
  std::string groupName() const override;
  std::string tagName() const override;
  std::string tagLabel() const override;
  uint16_t tag() const override;
  //! Return the IFD id as an integer. (Do not use, this is meant for library internal use.)
  int ifdId() const;
  //! Return the name of the IFD
  const char* ifdName() const;
  //! Return the index (unique id of this key within the original IFD)
  int idx() const;
  /*!
    @brief Write value to a data buffer and return the number
            of bytes written.

    The user must ensure that the buffer has enough memory. Otherwise
    the call results in undefined behaviour.

    @param buf Data buffer to write to.
    @param byteOrder Applicable byte order (little or big endian).
    @return Number of characters written.
  */
  long copy(byte* buf, ByteOrder byteOrder) const override;
  std::ostream& write(std::ostream& os, const ExifData* pMetadata = nullptr) const override;
  //! Return the type id of the value
  TypeId typeId() const override;
  //! Return the name of the type
  const char* typeName() const override;
  //! Return the size in bytes of one component of this type
  [[nodiscard]] size_t typeSize() const override;
  //! Return the number of components in the value
  [[nodiscard]] size_t count() const override;
  //! Return the size of the value in bytes
  [[nodiscard]] size_t size() const override;
  //! Return the value as a string.
  std::string toString() const override;
  std::string toString(long n) const override;
  [[nodiscard]] int64_t toInt64(size_t n = 0) const override;
  float toFloat(long n = 0) const override;
  Rational toRational(long n = 0) const override;
  Value::UniquePtr getValue() const override;
  const Value& value() const override;
  //! Return the size of the data area.
  long sizeDataArea() const;
  /*!
    @brief Return a copy of the data area of the value. The caller owns
            this copy and %DataBuf ensures that it will be deleted.

    Values may have a data area, which can contain additional
    information besides the actual value. This method is used to access
    such a data area.

    @return A %DataBuf containing a copy of the data area or an empty
            %DataBuf if the value does not have a data area assigned or the
            value is not set.
  */
  DataBuf dataArea() const;
  //@}

 private:
  // DATA
  ExifKey::UniquePtr key_;  //!< Key
  Value::UniquePtr value_;  //!< Value

};  // class Exifdatum

//! Container type to hold all metadata
typedef std::list<Exifdatum> ExifMetadata;

/*!
  @brief A container for Exif data.  This is a top-level class of the %Exiv2
          library. The container holds Exifdatum objects.

  Provide high-level access to the Exif data of an image:
  - read Exif information from JPEG files
  - access metadata through keys and standard C++ iterators
  - add, modify and delete metadata
  - write Exif data to JPEG files
  - extract Exif metadata to files, insert from these files
  - extract and delete Exif thumbnail (JPEG and TIFF thumbnails)
*/
class EXIV2API ExifData {
 public:
  //! ExifMetadata iterator type
  typedef ExifMetadata::iterator iterator;
  //! ExifMetadata const iterator type
  typedef ExifMetadata::const_iterator const_iterator;

  //! @name Manipulators
  //@{
  /*!
    @brief Returns a reference to the %Exifdatum that is associated with a
            particular \em key. If %ExifData does not already contain such
            an %Exifdatum, operator[] adds object \em Exifdatum(key).

    @note  Since operator[] might insert a new element, it can't be a const
            member function.
  */
  Exifdatum& operator[](const std::string& key);
  /*!
    @brief Add an Exifdatum from the supplied key and value pair.  This
            method copies (clones) key and value. No duplicate checks are
            performed, i.e., it is possible to add multiple metadata with
            the same key.
  */
  void add(const ExifKey& key, const Value* pValue);
  /*!
    @brief Add a copy of the \em exifdatum to the Exif metadata.  No
            duplicate checks are performed, i.e., it is possible to add
            multiple metadata with the same key.

    @throw Error if the makernote cannot be created
  */
  void add(const Exifdatum& exifdatum);
  /*!
    @brief Delete the Exifdatum at iterator position \em pos, return the
            position of the next exifdatum. Note that iterators into
            the metadata, including \em pos, are potentially invalidated
            by this call.
  */
  iterator erase(iterator pos);
  /*!
    @brief Remove all elements of the range \em beg, \em end, return the
            position of the next element. Note that iterators into
            the metadata are potentially invalidated by this call.
  */
  iterator erase(iterator beg, iterator end);
  /*!
    @brief Delete all Exifdatum instances resulting in an empty container.
            Note that this also removes thumbnails.
  */
  void clear();
  //! Sort metadata by key
  void sortByKey();
  //! Sort metadata by tag
  void sortByTag();
  //! Begin of the metadata
  iterator begin() {
    return exifMetadata_.begin();
  }
  //! End of the metadata
  iterator end() {
    return exifMetadata_.end();
  }
  /*!
    @brief Find the first Exifdatum with the given \em key, return an
            iterator to it.
  */
  iterator findKey(const ExifKey& key);
  //@}

  //! @name Accessors
  //@{
  //! Begin of the metadata
  const_iterator begin() const {
    return exifMetadata_.begin();
  }
  //! End of the metadata
  const_iterator end() const {
    return exifMetadata_.end();
  }
  /*!
    @brief Find the first Exifdatum with the given \em key, return a const
            iterator to it.
  */
  const_iterator findKey(const ExifKey& key) const;
  //! Return true if there is no Exif metadata
  bool empty() const {
    return count() == 0;
  }
  //! Get the number of metadata entries
  long count() const {
    return static_cast<long>(exifMetadata_.size());
  }
  //@}

 private:
  // DATA
  ExifMetadata exifMetadata_;

};  // class ExifData

/*!
  @brief Stateless parser class for Exif data. Images use this class to
          decode and encode binary Exif data.

  @note  Encode is lossy and is not the inverse of decode.
*/
class EXIV2API ExifParser {
 public:
  /*!
    @brief Decode metadata from a buffer \em pData of length \em size
            with binary Exif data to the provided metadata container.

            The buffer must start with a TIFF header. Return byte order
            in which the data is encoded.

    @param exifData Exif metadata container.
    @param pData 	  Pointer to the data buffer. Must point to data in
                    binary Exif format; no checks are performed.
    @param size 	  Length of the data buffer
    @return Byte order in which the data is encoded.
  */
  static ByteOrder decode(emscripten::val& exifData, const byte* pData, size_t size);

};  // class ExifParser

}  // namespace Exiv2

#endif  // #ifndef EXIF_HPP_
