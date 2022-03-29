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

#ifndef TIFFIMAGE_HPP_
#define TIFFIMAGE_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

// included header files
#include "image.hpp"

// *****************************************************************************
// namespace extensions
namespace Exiv2 {

// *****************************************************************************
// class definitions

// Add TIFF to the supported image formats
namespace ImageType {
const int tiff = 4; //!< TIFF image type (see class TiffImage)
const int dng = 4; //!< DNG image type (see class TiffImage)
const int nef = 4; //!< NEF image type (see class TiffImage)
const int pef = 4; //!< PEF image type (see class TiffImage)
const int arw = 4; //!< ARW image type (see class TiffImage)
const int sr2 = 4; //!< SR2 image type (see class TiffImage)
const int srw = 4; //!< SRW image type (see class TiffImage)
} // namespace ImageType

/*!
  @brief Class to access TIFF images. Exif metadata is
      supported directly, IPTC is read from the Exif data, if present.
  */
class EXIV2API TiffImage : public Image {
public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing TIFF image or create
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
  TiffImage(BasicIo::UniquePtr io, bool create);
  //@}

  //! @name Manipulators
  //@{
  void readMetadata() override;

  //! @name Accessors
  //@{
  std::string mimeType() const override;
  int pixelWidth() const override;
  int pixelHeight() const override;
  //@}

  //! @name NOT Implemented
  //@{
  //! Copy constructor
  TiffImage(const TiffImage& rhs) = delete;
  //! Assignment operator
  TiffImage& operator=(const TiffImage& rhs) = delete;
  //@}

private:
  //! @name Accessors
  //@{
  //! Return the group name of the group with the primary image.
  std::string primaryGroup() const;
  //@}

  // DATA
  mutable std::string primaryGroup_; //!< The primary group
  mutable std::string mimeType_; //!< The MIME type
  mutable int pixelWidthPrimary_; //!< Width of the primary image in pixels
  mutable int pixelHeightPrimary_; //!< Height of the primary image in pixels

}; // class TiffImage

/*!
  @brief Stateless parser class for data in TIFF format. Images use this
          class to decode and encode TIFF data. It is a wrapper of the
          internal class Internal::TiffParserWorker.
  */
class EXIV2API TiffParser {
public:
  /*!
    @brief Decode metadata from a buffer \em pData of length \em size
            with data in TIFF format to the provided metadata containers.

    @param exifData Exif metadata container.
    @param iptcData IPTC metadata container.
    @param xmpData  XMP metadata container.
    @param pData    Pointer to the data buffer. Must point to data in TIFF
                    format; no checks are performed.
    @param size     Length of the data buffer.

    @return Byte order in which the data is encoded.
  */
  static ByteOrder decode(emscripten::val& exifData, emscripten::val& iptcData, emscripten::val& xmpData, const byte* pData, uint32_t size);

}; // class TiffParser

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new TiffImage instance and return an auto-pointer to it.
          Caller owns the returned object and the auto-pointer ensures that
          it will be deleted.
  */
EXIV2API Image::UniquePtr newTiffInstance(BasicIo::UniquePtr io, bool create);

//! Check if the file iIo is a TIFF image.
EXIV2API bool isTiffType(BasicIo& iIo, bool advance);

} // namespace Exiv2

#endif // #ifndef TIFFIMAGE_HPP_
