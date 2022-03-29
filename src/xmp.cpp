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
 * along with this f; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301 USA.
 */
// *****************************************************************************
// included header files
#include "error.hpp"
#include "types.hpp"
#include "value.hpp"
#include "xmp_exiv2.hpp"

// Adobe XMP Toolkit
#define TXMP_STRING_TYPE std::string
#include "../externals/xmp/public/include/XMP.hpp"
#include "../externals/xmp/public/include/XMP.incl_cpp"

// *****************************************************************************
// class member definitions
namespace Exiv2 {

bool XmpParser::initialized_ = false;
XmpParser::XmpLockFct XmpParser::xmpLockFct_ = nullptr;
void* XmpParser::pLockData_ = nullptr;

int XmpParser::decode(emscripten::val& xmpData, const std::string& xmpPacket) {
  try {
    if (xmpPacket.empty())
      return 0;

    if (!initialize()) {
      return 2;
    }

    SXMPMeta meta(xmpPacket.data(), static_cast<XMP_StringLen>(xmpPacket.size()));
    std::string schemaNS, propPath, propVal;
    SXMPIterator iter(meta);
    while (iter.Next(&schemaNS, &propPath, &propVal)) {
      if (propPath.empty())
        continue;
      xmpData.set(propPath, propVal);
    }

    return 0;
  } catch (const XMP_Error& e) {
    EXV_ERROR << Error(kerXMPToolkitError, e.GetID(), e.GetErrMsg()) << "\n";
    return 3;
  }
}

bool XmpParser::initialize(XmpParser::XmpLockFct xmpLockFct, void* pLockData) {
  if (!initialized_) {
    xmpLockFct_ = xmpLockFct;
    pLockData_ = pLockData;
    initialized_ = SXMPMeta::Initialize();
  }
  return initialized_;
}

void XmpParser::terminate() {
  if (initialized_) {
    SXMPMeta::Terminate();
    initialized_ = false;
  }
}

} // namespace Exiv2