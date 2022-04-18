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
  File:      basicio.cpp
 */
// *****************************************************************************
// included header files
#include "basicio.hpp"

#include "config.h"
#include "datasets.hpp"
#include "error.hpp"
#include "image_int.hpp"
#include "types.hpp"

#define mode_t unsigned short

// *****************************************************************************
// class member definitions
namespace Exiv2 {
//! Internal Pimpl structure of class MemIo.
class MemIo::Impl final {
 public:
  Impl() = default;                   //!< Default constructor
  Impl(const byte* data, long size);  //!< Constructor 2

  // DATA
  byte* data_{nullptr};     //!< Pointer to the start of the memory area
  long idx_{0};             //!< Index into the memory area
  long size_{0};            //!< Size of the memory area
  long sizeAlloced_{0};     //!< Size of the allocated buffer
  bool isMalloced_{false};  //!< Was the buffer allocated?
  bool eof_{false};         //!< EOF indicator

  // METHODS
  void reserve(long wcount);  //!< Reserve memory

  // NOT IMPLEMENTED
  Impl(const Impl& rhs) = delete;             //!< Copy constructor
  Impl& operator=(const Impl& rhs) = delete;  //!< Assignment
};                                            // class MemIo::Impl

MemIo::Impl::Impl(const byte* data, long size) : data_(const_cast<byte*>(data)), size_(size) {
}

/*!
  @brief Utility class provides the block mapping to the part of data. This avoids allocating
        a single contiguous block of memory to the big data.
  */
class EXIV2API BlockMap {
 public:
  //! the status of the block.
  enum blockType_e { bNone, bKnown, bMemory };
  //! @name Creators
  //@{
  //! Default constructor. the init status of the block is bNone.
  BlockMap() = default;

  //! Destructor. Releases all managed memory.
  ~BlockMap() {
    delete[] data_;
  }

  //! @brief Populate the block.
  //! @param source The data populate to the block
  //! @param num The size of data
  void populate(byte* source, size_t num) {
    assert(source != nullptr);
    size_ = num;
    data_ = new byte[size_];
    type_ = bMemory;
    std::memcpy(data_, source, size_);
  }

  /*!
    @brief Change the status to bKnow. bKnow blocks do not contain the data,
          but they keep the size of data. This avoids allocating memory for parts
          of the file that contain image-date (non-metadata/pixel data) which never change in exiv2.
    @param num The size of the data
    */
  void markKnown(size_t num) {
    type_ = bKnown;
    size_ = num;
  }

  bool isNone() const {
    return type_ == bNone;
  }
  bool isInMem() const {
    return type_ == bMemory;
  }
  bool isKnown() const {
    return type_ == bKnown;
  }
  byte* getData() const {
    return data_;
  }
  size_t getSize() const {
    return size_;
  }

 private:
  blockType_e type_{bNone};
  byte* data_{nullptr};
  size_t size_{0};
};  // class BlockMap

void MemIo::Impl::reserve(long wcount) {
  const long need = wcount + idx_;
  long blockSize = 32 * 1024;  // 32768           `
  const long maxBlockSize = 4 * 1024 * 1024;

  if (!isMalloced_) {
    // Minimum size for 1st block
    long size = std::max(blockSize * (1 + need / blockSize), size_);
    auto data = static_cast<byte*>(std::malloc(size));
    if (data == nullptr) {
      throw Error(kerMallocFailed);
    }
    if (data_ != nullptr) {
      std::memcpy(data, data_, size_);
    }
    data_ = data;
    sizeAlloced_ = size;
    isMalloced_ = true;
  }

  if (need > size_) {
    if (need > sizeAlloced_) {
      blockSize = 2 * sizeAlloced_;
      if (blockSize > maxBlockSize)
        blockSize = maxBlockSize;
      // Allocate in blocks
      long want = blockSize * (1 + need / blockSize);
      data_ = static_cast<byte*>(std::realloc(data_, want));
      if (data_ == nullptr) {
        throw Error(kerMallocFailed);
      }
      sizeAlloced_ = want;
    }
    size_ = need;
  }
}

MemIo::MemIo() : p_(new Impl()) {
}

MemIo::MemIo(const byte* data, long size) : p_(new Impl(data, size)) {
}

MemIo::~MemIo() {
  if (p_->isMalloced_) {
    std::free(p_->data_);
  }
}

long MemIo::write(const byte* data, long wcount) {
  p_->reserve(wcount);
  assert(p_->isMalloced_);
  if (data != nullptr) {
    std::memcpy(&p_->data_[p_->idx_], data, wcount);
  }
  p_->idx_ += wcount;
  return wcount;
}

void MemIo::transfer(BasicIo& src) {
  auto memIo = dynamic_cast<MemIo*>(&src);
  if (memIo) {
    // Optimization if src is another instance of MemIo
    if (p_->isMalloced_) {
      std::free(p_->data_);
    }
    p_->idx_ = 0;
    p_->data_ = memIo->p_->data_;
    p_->size_ = memIo->p_->size_;
    p_->isMalloced_ = memIo->p_->isMalloced_;
    memIo->p_->idx_ = 0;
    memIo->p_->data_ = nullptr;
    memIo->p_->size_ = 0;
    memIo->p_->isMalloced_ = false;
  } else {
    // Generic reopen to reset position to start
    if (src.open() != 0) {
      throw Error(kerDataSourceOpenFailed, src.path());
    }
    p_->idx_ = 0;
    write(src);
    src.close();
  }
  if (error() || src.error())
    throw Error(kerMemoryTransferFailed);
}

long MemIo::write(BasicIo& src) {
  if (static_cast<BasicIo*>(this) == &src)
    return 0;
  if (!src.isopen())
    return 0;

  byte buf[4096];
  long readCount = 0;
  long writeTotal = 0;
  while ((readCount = src.read(buf, sizeof(buf)))) {
    write(buf, readCount);
    writeTotal += readCount;
  }

  return writeTotal;
}

int MemIo::putb(byte data) {
  p_->reserve(1);
  assert(p_->isMalloced_);
  p_->data_[p_->idx_++] = data;
  return data;
}

int MemIo::seek(long offset, Position pos) {
  long newIdx = 0;

  switch (pos) {
    case BasicIo::cur:
      newIdx = p_->idx_ + offset;
      break;
    case BasicIo::beg:
      newIdx = offset;
      break;
    case BasicIo::end:
      newIdx = p_->size_ + offset;
      break;
  }

  if (newIdx < 0)
    return 1;

  if (newIdx > p_->size_) {
    p_->eof_ = true;
    return 1;
  }

  p_->idx_ = newIdx;
  p_->eof_ = false;
  return 0;
}

byte* MemIo::mmap(bool /*isWriteable*/) {
  return p_->data_;
}

int MemIo::munmap() {
  return 0;
}

long MemIo::tell() const {
  return p_->idx_;
}

size_t MemIo::size() const {
  return p_->size_;
}

int MemIo::open() {
  p_->idx_ = 0;
  p_->eof_ = false;
  return 0;
}

bool MemIo::isopen() const {
  return true;
}

int MemIo::close() {
  return 0;
}

DataBuf MemIo::read(long rcount) {
  DataBuf buf(rcount);
  long readCount = read(buf.pData_, buf.size_);
  buf.size_ = readCount;
  return buf;
}

long MemIo::read(byte* buf, long rcount) {
  long avail = std::max(p_->size_ - p_->idx_, 0L);
  long allow = std::min(rcount, avail);
  std::memcpy(buf, &p_->data_[p_->idx_], allow);
  p_->idx_ += allow;
  if (rcount > avail)
    p_->eof_ = true;
  return allow;
}

int MemIo::getb() {
  if (p_->idx_ >= p_->size_) {
    p_->eof_ = true;
    return EOF;
  }
  return p_->data_[p_->idx_++];
}

int MemIo::error() const {
  return 0;
}

bool MemIo::eof() const {
  return p_->eof_;
}

std::string MemIo::path() const {
  return "MemIo";
}

void MemIo::populateFakeData() {
}

// *************************************************************************
// free functions
std::string ReplaceStringInPlace(std::string subject, const std::string& search, const std::string& replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
  return subject;
}
}  // namespace Exiv2
