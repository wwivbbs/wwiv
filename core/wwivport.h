/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services            */
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

#ifndef __INCLUDED_PLATFORM_WWIVPORT_H__
#define __INCLUDED_PLATFORM_WWIVPORT_H__

#if !defined(__unix__) && !defined(_WIN32) && defined(__APPLE__) && defined(__MACH__)
#define __unix__
#endif

// TODO(rushfan): This whole thing probably needs to be redone.
#if defined(_MSC_VER)
// Enable SAL attributes.
#ifndef _USE_ATTRIBUTES_FOR_SAL
#define _USE_ATTRIBUTES_FOR_SAL 1
#endif // _USE_ATTRIBUTES_FOR_SAL

#if (_MSC_VER < 1915)
#error "Visual Studio 2017 version 15.8.0 or later is required"
#endif // _MSC_VER < 1915

#endif // _MSC_VER

#if defined(__GNUC__) && !defined(__clang__)
#if (__GNUC__ < 6)
#error "GNC C++ 6.0 or later is required"
#endif // __GNUC__ < 6
#endif // __GNUC__

// WWIV's daten type is a 32-bit unsigned int. It can never be used for date
// arithemetic since negative values don't exist.  This will allow us to
// truncate a 64-bit time_t value for use till 2106.
#ifndef DATEN_T_DEFINED
#include <cstdint>
typedef uint32_t daten_t;
#define DATEN_T_DEFINED
#endif

#ifdef _MSC_VER
#ifdef _WIN64
typedef int64_t ssize_t;
#else
typedef int32_t ssize_t;
#endif // _WIN64
#endif // MSVC

#ifdef _WIN32
typedef int pid_t;
#endif // _WIN32

#endif // __INCLUDED_PLATFORM_WWIVPORT_H__
