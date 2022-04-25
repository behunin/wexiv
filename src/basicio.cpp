// SPDX-License-Identifier: GPL-2.0-or-later
// *****************************************************************************
// included header files
#include "basicio.hpp"

#include "config.h"
#include "datasets.hpp"
#include "enforce.hpp"
#include "error.hpp"
#include "image_int.hpp"
#include "types.hpp"

// + standard includes
#include <cstdlib>  // for alloc, realloc, free
#include <cstring>  // std::memcpy

#define mode_t unsigned short

// *****************************************************************************
namespace {
/// @brief replace each substring of the subject that matches the given search string with the given replacement.
void ReplaceStringInPlace(std::string& subject, std::string_view search, std::string_view replace) {
  auto pos = subject.find(search);
  while (pos != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += subject.find(search, pos + replace.length());
  }
}
}  // namespace

namespace Exiv2 {
void BasicIo::readOrThrow(byte* buf, size_t rcount, ErrorCode err) {
  const size_t nread = read(buf, rcount);
  enforce(nread == rcount, err);
  enforce(!error(), err);
}

void BasicIo::seekOrThrow(int64_t offset, Position pos, ErrorCode err) {
  const int r = seek(offset, pos);
  enforce(r == 0, err);
}
//! Internal Pimpl structure of class MemIo.
class MemIo::Impl final {
 public:
  Impl() = default;                     //!< Default constructor
  Impl(const byte* data, size_t size);  //!< Constructor 2
  ~Impl() = default;

  // DATA
  byte* data_{nullptr};     //!< Pointer to the start of the memory area
  size_t idx_{0};           //!< Index into the memory area
  size_t size_{0};          //!< Size of the memory area
  size_t sizeAlloced_{0};   //!< Size of the allocated buffer
  bool isMalloced_{false};  //!< Was the buffer allocated?
  bool eof_{false};         //!< EOF indicator

  // METHODS
  void reserve(size_t wcount);  //!< Reserve memory

  // NOT IMPLEMENTED
  Impl(const Impl& rhs) = delete;             //!< Copy constructor
  Impl& operator=(const Impl& rhs) = delete;  //!< Assignment
};                                            // class MemIo::Impl

MemIo::Impl::Impl(const byte* data, size_t size) : data_(const_cast<byte*>(data)), size_(size) {
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

void MemIo::Impl::reserve(size_t wcount) {
  const size_t need = wcount + idx_;
  size_t blockSize = 32 * 1024;  // 32768           `
  const size_t maxBlockSize = 4 * 1024 * 1024;

  if (!isMalloced_) {
    // Minimum size for 1st block
    size_t size = std::max(blockSize * (1 + need / blockSize), size_);
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
      size_t want = blockSize * (1 + need / blockSize);
      data_ = static_cast<byte*>(std::realloc(data_, want));
      if (data_ == nullptr) {
        throw Error(kerMallocFailed);
      }
      sizeAlloced_ = want;
    }
    size_ = need;
  }
}

MemIo::MemIo() : p_(std::make_unique<Impl>()) {
}

MemIo::MemIo(const byte* data, size_t size) : p_(std::make_unique<Impl>(data, size)) {
}

MemIo::~MemIo() {
  if (p_->isMalloced_) {
    std::free(p_->data_);
  }
}

size_t MemIo::write(const byte* data, size_t wcount) {
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

size_t MemIo::write(BasicIo& src) {
  if (static_cast<BasicIo*>(this) == &src)
    return 0;
  if (!src.isopen())
    return 0;

  byte buf[4096];
  size_t readCount = 0;
  size_t writeTotal = 0;
  while ((readCount = src.read(buf, sizeof(buf)))) {
    write(buf, readCount);
    writeTotal += readCount;
  }

  return writeTotal;
}

int MemIo::putb(byte data) {
  p_->reserve(1);
  p_->data_[p_->idx_++] = data;
  return data;
}

int MemIo::seek(int64_t offset, Position pos) {
  int64_t newIdx = 0;

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

  if (newIdx > static_cast<int64_t>(p_->size_)) {
    p_->eof_ = true;
    return 1;
  }

  p_->idx_ = static_cast<size_t>(newIdx);
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

DataBuf MemIo::read(size_t rcount) {
  DataBuf buf(rcount);
  size_t readCount = read(buf.data(), buf.size());
  buf.resize(readCount);
  return buf;
}

size_t MemIo::read(byte* buf, size_t rcount) {
  size_t avail = std::max(p_->size_ - p_->idx_, static_cast<size_t>(0));
  size_t allow = std::min(rcount, avail);
  if (allow > 0) {
    std::memcpy(buf, &p_->data_[p_->idx_], allow);
  }
  p_->idx_ += allow;
  if (rcount > avail) {
    p_->eof_ = true;
  }
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

const std::string& MemIo::path() const noexcept {
  static std::string _path{"MemIo"};
  return _path;
}

void MemIo::populateFakeData() {
}
}  // namespace Exiv2
