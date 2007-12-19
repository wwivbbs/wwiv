/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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


#if defined( WORDS_BIGENDIAN )
#define __BIG_ENDIAN__
#endif // WORDS_BIGENDIAN

//
// Sanity check the #defines
//

#if !defined( _WIN32 ) && !defined( __OS2__ ) && !defined( __unix__ ) && !defined( __MSDOS__ ) && !defined( __APPLE__ )
#error "Either _WIN32, __OS2__, or __unix__ must be defined"
#endif

#if defined( _WIN32 ) && defined(__OS2__)
#error "Either _WIN32 or __OS2__ must be defined, but NOT both!"
#endif

#if defined( _WIN32 ) && defined( __unix__ )
#error "Either _WIN32 or __unix__ must be defined, but NOT both!"
#endif

#if defined( __OS2__ ) && defined( __unix__ )
#error "Either __OS2__ or __unix__ must be defined, but not both!"
#endif




#endif // __INCLUDED_PLATFORM_TESTOS_H__
