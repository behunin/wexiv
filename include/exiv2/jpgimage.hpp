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

#ifndef JPGIMAGE_HPP_
#define JPGIMAGE_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

#include <array>

// included header files
#include "error.hpp"
#include "image.hpp"

// *****************************************************************************
// namespace extensions
namespace Exiv2 {

// *****************************************************************************
// class definitions

// Supported JPEG image formats
namespace ImageType {
const int jpeg = 1;  //!< JPEG image type (see class JpegImage)
const int exv = 2;   //!< EXV image type (see class ExvImage)
}  // namespace ImageType

/*!
      @brief Abstract helper base class to access JPEG images.
     */
class EXIV2API JpegBase : public Image {
 public:
  //! @name Manipulators
  //@{
  void readMetadata() override;
  //@}

  //! @name NOT implemented
  //@{
  //! Default constructor.
  JpegBase() = delete;
  //! Copy constructor
  JpegBase(const JpegBase& rhs) = delete;
  //! Assignment operator
  JpegBase& operator=(const JpegBase& rhs) = delete;
  //@}

 protected:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing image or create
        a new image from scratch. If a new image is to be created, any
        existing data is overwritten.
    @param type Image type.
    @param io An auto-pointer that owns a BasicIo instance used for
        reading and writing image metadata. \b Important: The constructor
        takes ownership of the passed in BasicIo instance through the
        auto-pointer. Callers should not continue to use the BasicIo
        instance after it is passed to this method.  Use the Image::io()
        method to get a temporary reference.
    */
  JpegBase(int type, BasicIo::UniquePtr io);
  //@}

  //! @name Accessors
  //@{
  /*!
    @brief Determine if the content of the BasicIo instance is of the
        type supported by this class.

    The advance flag determines if the read position in the stream is
    moved (see below). This applies only if the type matches and the
    function returns true. If the type does not match, the stream
    position is not changed. However, if reading from the stream fails,
    the stream position is undefined. Consult the stream state to obtain
    more information in this case.

    @param iIo BasicIo instance to read from.
    @param advance Flag indicating whether the position of the io
        should be advanced by the number of characters read to
        analyse the data (true) or left at its original
        position (false). This applies only if the type matches.
    @return  true  if the data matches the type of this class;<BR>
              false if the data does not match
    */
  virtual bool isThisType(BasicIo& iIo, bool advance) const = 0;
  //@}

 private:
  //! @name Accessors
  //@{
  /*!
    @brief Advances associated io instance to one byte past the next
        Jpeg marker and returns the marker. This method should be called
        when the BasicIo instance is positioned one byte past the end of a
        Jpeg segment.
    @param err the error code to throw if no marker is found
    @return the next Jpeg segment marker if successful;<BR>
            throws an Error if not successful
    */
  byte advanceToMarker(ErrorCode err) const;
  //@}

  /*!
    @brief Is the marker followed by a non-zero payload?
    @param marker The marker at the start of a segment
    @return true if the marker is followed by a non-zero payload
    */
  static bool markerHasLength(byte marker);
};  // class JpegBase

/*!
      @brief Class to access JPEG images
     */
class EXIV2API JpegImage : public JpegBase {
  friend EXIV2API bool isJpegType(BasicIo& iIo, bool advance);

 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing Jpeg image or create
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
  JpegImage(BasicIo::UniquePtr io);
  //@}
  //! @name Accessors
  //@{
  std::string mimeType() const override;
  //@}

  // NOT Implemented
  //! Default constructor
  JpegImage() = delete;
  //! Copy constructor
  JpegImage(const JpegImage& rhs) = delete;
  //! Assignment operator
  JpegImage& operator=(const JpegImage& rhs) = delete;

 protected:
  //! @name Accessors
  //@{
  bool isThisType(BasicIo& iIo, bool advance) const override;
  //@}

 private:
  // Constant data
  static const byte soi_;      // SOI marker
  static const byte blank_[];  // Minimal Jpeg image
};                             // class JpegImage

//! Helper class to access %Exiv2 files
class EXIV2API ExvImage : public JpegBase {
  friend EXIV2API bool isExvType(BasicIo& iIo, bool advance);

 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that can either open an existing EXV image or create
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
  ExvImage(BasicIo::UniquePtr io, bool create);
  //@}
  //! @name Accessors
  //@{
  std::string mimeType() const override;
  //@}

  // NOT Implemented
  //! Default constructor
  ExvImage() = delete;
  //! Copy constructor
  ExvImage(const ExvImage& rhs) = delete;
  //! Assignment operator
  ExvImage& operator=(const ExvImage& rhs) = delete;

 protected:
  //! @name Accessors
  //@{
  bool isThisType(BasicIo& iIo, bool advance) const override;
  //@}

 private:
  // Constant data
  static const char exiv2Id_[];  // EXV identifier
  static const byte blank_[];    // Minimal exiv2 file

};  // class ExvImage

// *****************************************************************************
// template, inline and free functions

// These could be static private functions on Image subclasses but then
// ImageFactory needs to be made a friend.
/*!
  @brief Create a new JpegImage instance and return an auto-pointer to it.
          Caller owns the returned object and the auto-pointer ensures that
          it will be deleted.
  */
EXIV2API Image::UniquePtr newJpegInstance(BasicIo::UniquePtr io, bool create);
//! Check if the file iIo is a JPEG image.
EXIV2API bool isJpegType(BasicIo& iIo, bool advance);

}  // namespace Exiv2

#endif  // #ifndef JPGIMAGE_HPP_
