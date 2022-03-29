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
// *****************************************************************************

#include "version.hpp"

#include "config.h"
#include "makernote_int.hpp"

#ifndef lengthof
#define lengthof(x) sizeof(x) / sizeof(x[0])
#endif
#ifndef _MAX_PATH
#define _MAX_PATH 512
#endif

namespace Exiv2 {
int versionNumber() {
  return EXIV2_MAKE_VERSION(EXIV2_MAJOR_VERSION, EXIV2_MINOR_VERSION, EXIV2_PATCH_VERSION);
}

std::string versionString() {
  std::ostringstream os;
  os << EXIV2_MAJOR_VERSION << '.' << EXIV2_MINOR_VERSION << '.' << EXIV2_PATCH_VERSION;
  return os.str();
}

std::string versionNumberHexString() {
  std::ostringstream os;
  os << std::hex << std::setw(6) << std::setfill('0') << Exiv2::versionNumber();
  return os.str();
}

const char* version() {
  return EXV_PACKAGE_VERSION;
}

bool testVersion(int major, int minor, int patch) {
  return versionNumber() >= EXIV2_MAKE_VERSION(major, minor, patch);
}
} // namespace Exiv2

static bool shouldOutput(const std::vector<std::regex>& greps, const char* key, const std::string& value) {
  bool bPrint = greps.empty();
  for (auto const& g : greps) {
    bPrint = std::regex_search(key, g) || std::regex_search(value, g);
    if (bPrint) {
      break;
    }
  }
  return bPrint;
}

static void output(std::ostream& os, const std::vector<std::regex>& greps, const char* name, const std::string& value) {
  if (shouldOutput(greps, name, value))
    os << name << "=" << value << std::endl;
}

static void output(std::ostream& os, const std::vector<std::regex>& greps, const char* name, int value) {
  std::ostringstream stringStream;
  stringStream << value;
  output(os, greps, name, stringStream.str());
}
