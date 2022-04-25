// SPDX-License-Identifier: GPL-2.0-or-later

// *****************************************************************************
// included header files
#include "error.hpp"
#include "types.hpp"
#include "value.hpp"
#include "xmp_exiv2.hpp"

// + standard includes
#include <algorithm>
#include <iostream>

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
      EXV_ERROR << "XMP toolkit initialization failed.\n";
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

}  // namespace Exiv2