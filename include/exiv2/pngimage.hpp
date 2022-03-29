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
#ifndef PNGIMAGE_HPP_
#define PNGIMAGE_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

// included header files
#include "image.hpp"

// *****************************************************************************
// namespace extensions
namespace Exiv2 {

// *****************************************************************************
// class definitions

// Add PNG to the supported image formats
namespace ImageType {
const int png = 6; //!< PNG image type (see class PngImage)
}

/*!
  @brief Class to access PNG images. Exif and IPTC metadata are supported
      directly.
  */
class EXIV2API PngImage : public Image {
public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing PNG image or create
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
    */
  PngImage(BasicIo::UniquePtr io);
  //@}

  //! @name Manipulators
  //@{
  void readMetadata() override;
  //@}

  //! @name Accessors
  //@{
  std::string mimeType() const override;
  //@}

  //! @name NOT implemented
  //@{
  //! Copy constructor
  PngImage(const PngImage& rhs) = delete;
  //! Assignment operator
  PngImage& operator=(const PngImage& rhs) = delete;
  //@}

private:
  std::string profileName_;

}; // class PngImage

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new PngImage instance and return an auto-pointer to it.
          Caller owns the returned object and the auto-pointer ensures that
             it will be deleted.
     */
EXIV2API Image::UniquePtr newPngInstance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a PNG image.
EXIV2API bool isPngType(BasicIo& iIo, bool advance);

} // namespace Exiv2

#endif // #ifndef PNGIMAGE_HPP_
