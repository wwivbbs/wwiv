/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services            */
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

#ifndef INCLUDED_CORE_WWIVPORT_H
#define INCLUDED_CORE_WWIVPORT_H

// WWIV's daten type is a 32-bit unsigned int. It can never be used for date
// arithmetic since negative values don't exist.  This will allow us to
// truncate a 64-bit time_t value for use till 2106.
#ifndef DATEN_T_DEFINED
#include <cstdint>
typedef uint32_t daten_t;
#define DATEN_T_DEFINED
#endif

#if !defined(__unix__) && !defined(_WIN32) && defined(__APPLE__) && defined(__MACH__)
#define __unix__
#endif

#if defined(_MSC_VER)
#if (_MSC_VER < 1932)
// See https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros
#error "Visual Studio 2022 version 17.2.0 or later is required"
#endif // _MSC_VER < 1932

#ifdef _WIN64
typedef int64_t ssize_t;
#else
typedef int ssize_t;
#endif // _WIN64

#ifdef _WIN32
typedef int pid_t;
#endif // _WIN32

#endif // _MSC_VER

#if defined(__GNUC__) && !defined(__clang__)
#if (__GNUC__ < 8)
#error "GNC C++ 8.3 or later is required"
#endif // __GNUC__ < 8
#endif // __GNUC__

#endif
