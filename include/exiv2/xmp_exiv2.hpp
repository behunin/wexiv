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
#ifndef XMP_HPP_
#define XMP_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

#include <emscripten/val.h>

// *****************************************************************************
namespace Exiv2 {

// *****************************************************************************
class ExifData;

// *****************************************************************************

/*!
  @brief Stateless parser class for XMP packets. Images use this
          class to parse and serialize XMP packets. The parser uses
          the XMP toolkit to do the job.
*/
class EXIV2API XmpParser {
 public:
  /*!
    @brief Decode XMP metadata from an XMP packet \em xmpPacket into
            \em xmpData. The format of the XMP packet must follow the
            XMP specification. This method clears any previous contents
            of \em xmpData.

    @param xmpData   Container for the decoded XMP properties
    @param xmpPacket The raw XMP packet to decode
    @return 0 if successful;<BR>
            1 if XMP support has not been compiled-in;<BR>
            2 if the XMP toolkit failed to initialize;<BR>
            3 if the XMP toolkit failed and raised an XMP_Error
  */
  static int decode(emscripten::val& xmpData, const std::string& xmpPacket);
  /*!
    @brief Lock/unlock function type

    A function of this type can be passed to initialize() to
    make subsequent registration of XMP namespaces thread-safe.
    See the initialize() function for more information.

    @param pLockData Pointer to the pLockData passed to initialize()
    @param lockUnlock Indicates whether to lock (true) or unlock (false)
  */
  typedef void (*XmpLockFct)(void* pLockData, bool lockUnlock);

  /*!
    @brief Initialize the XMP Toolkit.

    Calling this method is usually not needed, as encode() and
    decode() will initialize the XMP Toolkit if necessary.

    The function takes optional pointers to a callback function
    \em xmpLockFct and related data \em pLockData that the parser
    uses when XMP namespaces are subsequently registered.

    The initialize() function itself still is not thread-safe and
    needs to be called in a thread-safe manner (e.g., on program
    startup), but if used with suitable additional locking
    parameters, any subsequent registration of namespaces will be
    thread-safe.

    Example usage on Windows using a critical section:

    @code
    void main()
    {
        struct XmpLock
        {
            CRITICAL_SECTION cs;
            XmpLock()  { InitializeCriticalSection(&cs); }
            ~XmpLock() { DeleteCriticalSection(&cs); }

            static void LockUnlock(void* pData, bool fLock)
            {
                XmpLock* pThis = reinterpret_cast<XmpLock*>(pData);
                if (pThis)
                {
                    (fLock) ? EnterCriticalSection(&pThis->cs)
                            : LeaveCriticalSection(&pThis->cs);
                }
            }
        } xmpLock;

        // Pass the locking mechanism to the XMP parser on initialization.
        // Note however that this call itself is still not thread-safe.
        Exiv2::XmpParser::initialize(XmpLock::LockUnlock, &xmpLock);

        // Program continues here, subsequent registrations of XMP
        // namespaces are serialized using xmpLock.

    }
    @endcode

    @return True if the initialization was successful, else false.
  */
  static bool initialize(XmpParser::XmpLockFct xmpLockFct = nullptr, void* pLockData = nullptr);
  /*!
    @brief Terminate the XMP Toolkit and unregister custom namespaces.

    Call this method when the XmpParser is no longer needed to
    allow the XMP Toolkit to cleanly shutdown.
  */
  static void terminate();

 private:
  // DATA
  static bool initialized_;  //! Indicates if the XMP Toolkit has been initialized
  static XmpLockFct xmpLockFct_;
  static void* pLockData_;

};  // class XmpParser

}  // namespace Exiv2

#endif  // #ifndef XMP_HPP_
