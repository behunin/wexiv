// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef RW2IMAGE_INT_HPP_
#define RW2IMAGE_INT_HPP_

// *****************************************************************************
// included header files
#include "tiffimage_int.hpp"

namespace Exiv2 {
namespace Internal {

/*!
  @brief Panasonic RW2 header structure.
*/
class Rw2Header : public TiffHeaderBase {
 public:
  //! @name Creators
  //@{
  //! Default constructor
  Rw2Header();
  //! Destructor.
  ~Rw2Header() override = default;
  //@}

};  // class Rw2Header

}  // namespace Internal
}  // namespace Exiv2

#endif  // #ifndef RW2IMAGE_INT_HPP_
