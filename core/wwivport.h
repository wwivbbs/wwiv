/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services            */
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
#if __GNUC__ >= 3
#define __noinline __attribute__((noinline))
#define __pure __attribute__((pure))
#define __const __attribute__((const))
#define __noreturn __attribute__((noreturn))
#define __malloc __attribute__((malloc))
#define __must_check __attribute__((warn_unused_result))
#define __deprecated __attribute__((deprecated))
#define __used __attribute__((used))
// This causes some compile errors on CentOS and others.
// See https://bugzilla.novell.com/show_bug.cgi?id=895495
//#define __unused       __attribute__ ((unused))
#define __packed __attribute__((packed))
#define __align(x) __attribute__((aligned(x)))
#define __align_max __attribute__((aligned))
#define __printf __attribute__((printf(printf, 1, 2)))
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)

// Enable SAL attributes.
#ifndef _USE_ATTRIBUTES_FOR_SAL
#define _USE_ATTRIBUTES_FOR_SAL 1
#endif // _USE_ATTRIBUTES_FOR_SAL

#define __noinline /* no noinline */
#define __pure     /* no pure */
#define __const    /* no const */
#define __noreturn /* no noreturn */
#define __malloc   /* no malloc */
#define __must_check _Check_return_
#define __deprecated /* no deprecated */
#define __used       /* no used */
#define __unused     /* no unused */
#define __packed     /* no packed */
#define __align(x)   /* no aligned */
#define __align_max  /* no align_max */
#define __printf _Printf_format_string_
#define likely(x) (x)
#define unlikely(x) (x)
#else
#define __noinline   /* no noinline */
#define __pure       /* no pure */
#define __const      /* no const */
#define __noreturn   /* no noreturn */
#define __malloc     /* no malloc */
#define __must_check /* no warn_unused_result */
#define __deprecated /* no deprecated */
#define __used       /* no used */
#define __unused     /* no unused */
#define __packed     /* no packed */
#define __align(x)   /* no aligned */
#define __align_max  /* no align_max */
#define __printf     /* no printf */
#define likely(x) (x)
#define unlikely(x) (x)
#endif

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

#else // !_WIN32

#endif // _WIN32

#endif // __INCLUDED_PLATFORM_WWIVPORT_H__
