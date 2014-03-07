/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/

#ifndef __INCLUDED_WTYPES_H__
#define __INCLUDED_WTYPES_H__

//
// Defined for all platforms
//

// This should only be used in files, where size matters, not for
// function return values, etc.

typedef unsigned char       BYTE;
typedef short               INT16;
typedef unsigned short      UINT16;

//
// Defined on everything except for WIN32
//
#if !defined (_WIN32)
typedef unsigned char       UCHAR;
typedef unsigned short      WORD;
typedef unsigned long       DWORD;
typedef unsigned long       DWORD32;

typedef unsigned short      USHORT;
typedef unsigned int        UINT;
typedef int                 INT;
typedef long                LONG32;

#ifndef LOBYTE
#define LOBYTE(w)           ((BYTE)(w))
#endif  // LOBYTE

#ifndef HIBYTE
#define HIBYTE(w)           ((BYTE)(((WORD)(w) >> 8) & 0xFF))
#endif  // HIBYTE

#endif // !_WIN32

#endif	// __INCLUDED_WTYPES_H__
