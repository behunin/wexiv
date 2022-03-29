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
  File:      basicio.hpp
 */
#ifndef BASICIO_HPP_
#define BASICIO_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

// included header files
#include "types.hpp"

// + standard includes
#include <memory>

// *****************************************************************************
// namespace extensions
namespace Exiv2 {

// *****************************************************************************
// class definitions

/*!
  @brief An interface for simple binary IO.

  Designed to have semantics and names similar to those of C style FILE*
  operations. Subclasses should all behave the same so that they can be
  interchanged.
  */
class EXIV2API BasicIo {
public:
  //! BasicIo auto_ptr type
  typedef std::unique_ptr<BasicIo> UniquePtr;

  //! Seek starting positions
  enum Position { beg, cur, end };

  //! @name Creators
  //@{
  //! Destructor
  virtual ~BasicIo() = default;
  //@}

  //! @name Manipulators
  //@{
  /*!
    @brief Open the IO source using the default access mode. The
        default mode should allow for reading and writing.

    This method can also be used to "reopen" an IO source which will
    flush any unwritten data and reset the IO position to the start.
    Subclasses may provide custom methods to allow for
    opening IO sources differently.

    @return 0 if successful;<BR>
        Nonzero if failure.
    */
  virtual int open() = 0;

  /*!
    @brief Close the IO source. After closing a BasicIo instance can not
        be read or written. Closing flushes any unwritten data. It is
        safe to call close on a closed instance.
    @return 0 if successful;<BR>
        Nonzero if failure.
    */
  virtual int close() = 0;
  /*!
    @brief Write data to the IO source. Current IO position is advanced
        by the number of bytes written.
    @param data Pointer to data. Data must be at least \em wcount
        bytes long
    @param wcount Number of bytes to be written.
    @return Number of bytes written to IO source successfully;<BR>
        0 if failure;
    */
  virtual long write(const byte* data, long wcount) = 0;
  /*!
    @brief Write data that is read from another BasicIo instance to
        the IO source. Current IO position is advanced by the number
        of bytes written.
    @param src Reference to another BasicIo instance. Reading start
        at the source's current IO position
    @return Number of bytes written to IO source successfully;<BR>
        0 if failure;
    */
  virtual long write(BasicIo& src) = 0;
  /*!
    @brief Write one byte to the IO source. Current IO position is
        advanced by one byte.
    @param data The single byte to be written.
    @return The value of the byte written if successful;<BR>
        EOF if failure;
    */
  virtual int putb(byte data) = 0;
  /*!
    @brief Read data from the IO source. Reading starts at the current
        IO position and the position is advanced by the number of bytes
        read.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return DataBuf instance containing the bytes read. Use the
        DataBuf::size_ member to find the number of bytes read.
        DataBuf::size_ will be 0 on failure.
    */
  virtual DataBuf read(long rcount) = 0;
  /*!
    @brief Read data from the IO source. Reading starts at the current
        IO position and the position is advanced by the number of bytes
        read.
    @param buf Pointer to a block of memory into which the read data
        is stored. The memory block must be at least \em rcount bytes
        long.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return Number of bytes read from IO source successfully;<BR>
        0 if failure;
    */
  virtual long read(byte* buf, long rcount) = 0;
  /*!
    @brief Read one byte from the IO source. Current IO position is
        advanced by one byte.
    @return The byte read from the IO source if successful;<BR>
        EOF if failure;
    */
  virtual int getb() = 0;
  /*!
    @brief Remove all data from this object's IO source and then transfer
        data from the \em src BasicIo object into this object.

    The source object is invalidated by this operation and should not be
    used after this method returns. This method exists primarily to
    be used with the BasicIo::temporary() method.

    @param src Reference to another BasicIo instance. The entire contents
        of src are transferred to this object. The \em src object is
        invalidated by the method.
    @throw Error In case of failure
    */
  virtual void transfer(BasicIo& src) = 0;
  /*!
    @brief Move the current IO position.
    @param offset Number of bytes to move the position relative
        to the starting position specified by \em pos
    @param pos Position from which the seek should start
    @return 0 if successful;<BR>
        Nonzero if failure;
    */
  virtual int seek(long offset, Position pos) = 0;

  /*!
    @brief Direct access to the IO data. For files, this is done by
            mapping the file into the process's address space; for memory
            blocks, this allows direct access to the memory block.
    @param isWriteable Set to true if the mapped area should be writeable
            (default is false).
    @return A pointer to the mapped area.
    @throw Error In case of failure.
    */
  virtual byte* mmap(bool isWriteable = false) = 0;
  /*!
    @brief Remove a mapping established with mmap(). If the mapped area
            is writeable, this ensures that changes are written back.
    @return 0 if successful;<BR>
            Nonzero if failure;
    */
  virtual int munmap() = 0;

  //@}

  //! @name Accessors
  //@{
  /*!
    @brief Get the current IO position.
    @return Offset from the start of IO if successful;<BR>
            -1 if failure;
    */
  virtual long tell() const = 0;
  /*!
    @brief Get the current size of the IO source in bytes.
    @return Size of the IO source in bytes;<BR>
            -1 if failure;
    */
  virtual size_t size() const = 0;
  //!Returns true if the IO source is open, otherwise false.
  virtual bool isopen() const = 0;
  //!Returns 0 if the IO source is in a valid state, otherwise nonzero.
  virtual int error() const = 0;
  //!Returns true if the IO position has reached the end, otherwise false.
  virtual bool eof() const = 0;
  /*!
    @brief Return the path to the IO resource. Often used to form
        comprehensive error messages where only a BasicIo instance is
        available.
    */
  virtual std::string path() const = 0;

  /*!
    @brief Mark all the bNone blocks to bKnow. This avoids allocating memory
      for parts of the file that contain image-date (non-metadata/pixel data)

    @note This method should be only called after the concerned data (metadata)
          are all downloaded from the remote file to memory.
    */
  virtual void populateFakeData() {}

  /*!
    @brief this is allocated and populated by mmap()
    */
  byte* bigBlock_{};

  //@}
}; // class BasicIo

/*!
  @brief Utility class that closes a BasicIo instance upon destruction.
      Meant to be used as a stack variable in functions that need to
      ensure BasicIo instances get closed. Useful when functions return
      errors from many locations.
  */
class EXIV2API IoCloser {
public:
  //! @name Creators
  //@{
  //! Constructor, takes a BasicIo reference
  explicit IoCloser(BasicIo& bio) : bio_(bio) {}
  //! Destructor, closes the BasicIo reference
  virtual ~IoCloser() { close(); }
  //@}

  //! @name Manipulators
  //@{
  //! Close the BasicIo if it is open
  void close() {
    if (bio_.isopen())
      bio_.close();
  }
  //@}

  // DATA
  //! The BasicIo reference
  BasicIo& bio_;

  // Not implemented
  //! Copy constructor
  IoCloser(const IoCloser&) = delete;
  //! Assignment operator
  IoCloser& operator=(const IoCloser&) = delete;
}; // class IoCloser

/*!
  @brief Provides binary IO on blocks of memory by implementing the BasicIo
      interface. A copy-on-write implementation ensures that the data passed
      in is only copied when necessary, i.e., as soon as data is written to
      the MemIo. The original data is only used for reading. If writes are
      performed, the changed data can be retrieved using the read methods
      (since the data used in construction is never modified).

  @note If read only usage of this class is common, it might be worth
      creating a specialized readonly class or changing this one to
      have a readonly mode.
  */
class EXIV2API MemIo : public BasicIo {
public:
  //! @name Creators
  //@{
  //! Default constructor that results in an empty object
  MemIo();
  /*!
    @brief Constructor that accepts a block of memory. A copy-on-write
        algorithm allows read operations directly from the original data
        and will create a copy of the buffer on the first write operation.
    @param data Pointer to data. Data must be at least \em size
        bytes long
    @param size Number of bytes to copy.
    */
  MemIo(const byte* data, long size);
  //! Destructor. Releases all managed memory
  ~MemIo() override;
  //@}

  //! @name Manipulators
  //@{
  /*!
    @brief Memory IO is always open for reading and writing. This method
            therefore only resets the IO position to the start.

    @return 0
    */
  int open() override;
  /*!
    @brief Does nothing on MemIo objects.
    @return 0
    */
  int close() override;
  /*!
    @brief Write data to the memory block. If needed, the size of the
        internal memory block is expanded. The IO position is advanced
        by the number of bytes written.
    @param data Pointer to data. Data must be at least \em wcount
        bytes long
    @param wcount Number of bytes to be written.
    @return Number of bytes written to the memory block successfully;<BR>
            0 if failure;
    */
  long write(const byte* data, long wcount) override;
  /*!
    @brief Write data that is read from another BasicIo instance to
        the memory block. If needed, the size of the internal memory
        block is expanded. The IO position is advanced by the number
        of bytes written.
    @param src Reference to another BasicIo instance. Reading start
        at the source's current IO position
    @return Number of bytes written to the memory block successfully;<BR>
            0 if failure;
    */
  long write(BasicIo& src) override;
  /*!
    @brief Write one byte to the memory block. The IO position is
        advanced by one byte.
    @param data The single byte to be written.
    @return The value of the byte written if successful;<BR>
            EOF if failure;
    */
  int putb(byte data) override;
  /*!
    @brief Read data from the memory block. Reading starts at the current
        IO position and the position is advanced by the number of
        bytes read.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return DataBuf instance containing the bytes read. Use the
          DataBuf::size_ member to find the number of bytes read.
          DataBuf::size_ will be 0 on failure.
    */
  DataBuf read(long rcount) override;
  /*!
    @brief Read data from the memory block. Reading starts at the current
        IO position and the position is advanced by the number of
        bytes read.
    @param buf Pointer to a block of memory into which the read data
        is stored. The memory block must be at least \em rcount bytes
        long.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return Number of bytes read from the memory block successfully;<BR>
            0 if failure;
    */
  long read(byte* buf, long rcount) override;
  /*!
    @brief Read one byte from the memory block. The IO position is
        advanced by one byte.
    @return The byte read from the memory block if successful;<BR>
            EOF if failure;
    */
  int getb() override;
  /*!
    @brief Clear the memory block and then transfer data from
        the \em src BasicIo object into a new block of memory.

    This method is optimized to simply swap memory block if the source
    object is another MemIo instance. The source BasicIo instance
    is invalidated by this operation and should not be used after this
    method returns. This method exists primarily to be used with
    the BasicIo::temporary() method.

    @param src Reference to another BasicIo instance. The entire contents
        of src are transferred to this object. The \em src object is
        invalidated by the method.
    @throw Error In case of failure
    */
  void transfer(BasicIo& src) override;
  /*!
    @brief Move the current IO position.
    @param offset Number of bytes to move the IO position
        relative to the starting position specified by \em pos
    @param pos Position from which the seek should start
    @return 0 if successful;<BR>
            Nonzero if failure;
    */
  int seek(long offset, Position pos) override;
  /*!
    @brief Allow direct access to the underlying data buffer. The buffer
            is not protected against write access in any way, the argument
            is ignored.
    @note  The application must ensure that the memory pointed to by the
            returned pointer remains valid and allocated as long as the
            MemIo object exists.
    */
  byte* mmap(bool /*isWriteable*/ = false) override;
  int munmap() override;
  //@}

  //! @name Accessors
  //@{
  /*!
    @brief Get the current IO position.
    @return Offset from the start of the memory block
    */
  long tell() const override;
  /*!
    @brief Get the current memory buffer size in bytes.
    @return Size of the in memory data in bytes;<BR>
            -1 if failure;
    */
  size_t size() const override;
  //!Always returns true
  bool isopen() const override;
  //!Always returns 0
  int error() const override;
  //!Returns true if the IO position has reached the end, otherwise false.
  bool eof() const override;
  //! Returns a dummy path, indicating that memory access is used
  std::string path() const override;

  /*!
    @brief Mark all the bNone blocks to bKnow. This avoids allocating memory
      for parts of the file that contain image-date (non-metadata/pixel data)

    @note This method should be only called after the concerned data (metadata)
          are all downloaded from the remote file to memory.
    */
  void populateFakeData() override;

  //@}

  // NOT IMPLEMENTED
  //! Copy constructor
  MemIo(MemIo& rhs) = delete;
  //! Assignment operator
  MemIo& operator=(const MemIo& rhs) = delete;

private:
  // Pimpl idiom
  class Impl;
  std::unique_ptr<Impl> p_;

}; // class MemIo

// *****************************************************************************
// template, inline and free functions
/*!
      @brief replace each substring of the subject that matches the given search string with the given replacement.
      @return the subject after replacing.
     */
EXIV2API std::string ReplaceStringInPlace(std::string subject, const std::string& search, const std::string& replace);
} // namespace Exiv2
#endif // #ifndef BASICIO_HPP_
