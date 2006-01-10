/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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

#ifndef __INCLUDED_PLATFORM_TESTOS_H__
#define __INCLUDED_PLATFORM_TESTOS_H__


//
// Define _UNIX if "linux" is defined
//
#if defined (linux)
#define _UNIX
#endif // defined (linux)


//
// Sanity check the #defines
//

#if !defined(_WIN32) && !defined(__OS2__) && !defined(_UNIX) && !defined(__MSDOS__)
#error "Either _WIN32, __OS2__, or _UNIX must be defined"
#endif

#if defined(_WIN32) && defined(__OS2__)
#error "Either _WIN32 or __OS2__ must be defined, but NOT both!"
#endif

#if defined(_WIN32) && defined(_UNIX)
#error "Either _WIN32 or _UNIX must be defined, but NOT both!"
#endif

#if defined(__OS2__) && defined(_UNIX)
#error "Either __OS2__ or _UNIX must be defined, but not both!"
#endif




#endif // __INCLUDED_PLATFORM_TESTOS_H__
