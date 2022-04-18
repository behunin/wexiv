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
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <exiv2/wexiv.hpp>

// include local header files which are not part of libexiv2
#include "i18n.h"  // NLS support.
#include "utils.hpp"
#include "xmp_exiv2.hpp"
using namespace emscripten;

#ifdef __cplusplus
extern "C" {
#endif
// *****************************************************************************

extern void db_store(const char* name, EM_VAL exif_handle, EM_VAL iptc_handle, EM_VAL xmp_handle, EM_VAL head_handle);

Exiv2::Image::UniquePtr image;

int EMSCRIPTEN_KEEPALIVE getmeta(unsigned char* arr, long length, const char* name) {
  Exiv2::XmpParser::initialize();

#ifdef EXV_ENABLE_NLS
  setlocale(LC_ALL, "");
  const std::string localeDir =
      EXV_LOCALEDIR[0] == '/' ? EXV_LOCALEDIR : (Exiv2::getProcessPath() + EXV_SEPARATOR_STR + EXV_LOCALEDIR);
  bindtextdomain(EXV_PACKAGE_NAME, localeDir.c_str());
  textdomain(EXV_PACKAGE_NAME);
#endif

  try {
    image = Exiv2::ImageFactory::open(arr, length);
    image->readMetadata();

    db_store(name, image->exifData().as_handle(), image->iptcData().as_handle(), image->xmpData().as_handle(),
             image->headData().as_handle());

  } catch (Exiv2::Error& e) {
    std::string str{e.what()};
    emscripten_log(EM_LOG_ERROR, "%s", &str);
    return e.code();
  }

  Exiv2::XmpParser::terminate();

  return 0;
}

void EMSCRIPTEN_KEEPALIVE clearimage() {
  image.reset();
}

#ifdef __cplusplus
}
#endif
