// SPDX-License-Identifier: GPL-2.0-or-later

#include "metadatum.hpp"

namespace Exiv2 {
Key::UniquePtr Key::clone() const {
  return UniquePtr(clone_());
}

uint32_t Metadatum::toUint32(size_t n) const {
  return static_cast<uint32_t>(toInt64(n));
}

}  // namespace Exiv2
