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
  @file    cr2image.hpp
  @brief   Class Cr2Image
  @author  Andreas Huggel (ahu)
           <a href="mailto:ahuggel@gmx.net">ahuggel@gmx.net</a>
  @date    22-Apr-06, ahu: created
 */
#ifndef CR2IMAGE_HPP_
#define CR2IMAGE_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

// included header files
#include "image.hpp"

// *****************************************************************************
// namespace extensions
namespace Exiv2 {

// *****************************************************************************
// class definitions

// Add CR2 to the supported image formats
namespace ImageType {
const int cr2 = 7; //!< CR2 image type (see class Cr2Image)
}

/*!
  @brief Class to access raw Canon CR2 images.  Exif metadata
      is supported directly, IPTC is read from the Exif data, if present.
  */
class EXIV2API Cr2Image : public Image {
public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing CR2 image or create
        a new image from scratch. If a new image is to be created, any
        existing data is overwritten. Since the constructor can not return
        a result, callers should check the good() method after object
        construction to determine success or failure.
    @param io An auto-pointer that owns a BasicIo instance used for
        reading and writing image metadata. \b Important: The constructor
        takes ownership of the passed in BasicIo instance through the
        auto-pointer. Callers should not continue to use the BasicIo
        instance after it is passed to this method.  Use the Image::io()
        method to get a temporary reference.
    @param create Specifies if an existing image should be read (false)
        or if a new file should be created (true).
    */
  Cr2Image(BasicIo::UniquePtr io, bool create);
  //@}

  //! @name Manipulators
  //@{
  void readMetadata() override;
  //@}

  //! @name Accessors
  //@{
  std::string mimeType() const override;
  int pixelWidth() const override;
  int pixelHeight() const override;
  //@}

  //! @name NOT implemented
  //@{
  //! Copy constructor
  Cr2Image(const Cr2Image& rhs) = delete;
  //! Assignment operator
  Cr2Image& operator=(const Cr2Image& rhs) = delete;
  //@}

}; // class Cr2Image

/*!
  @brief Stateless parser class for data in CR2 format. Images use this
          class to decode and encode CR2 data.
          See class TiffParser for details.
  */
class EXIV2API Cr2Parser {
public:
  /*!
    @brief Decode metadata from a buffer \em pData of length \em size
            with data in CR2 format to the provided metadata containers.
            See TiffParser::decode().
  */
  static ByteOrder decode(emscripten::val& exifData, emscripten::val& iptcData, emscripten::val& xmpData, const byte* pData, uint32_t size);

}; // class Cr2Parser

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
      @brief Create a new Cr2Image instance and return an auto-pointer to it.
             Caller owns the returned object and the auto-pointer ensures that
             it will be deleted.
     */
EXIV2API Image::UniquePtr newCr2Instance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a CR2 image.
EXIV2API bool isCr2Type(BasicIo& iIo, bool advance);

} // namespace Exiv2

#endif // #ifndef CR2IMAGE_HPP_
