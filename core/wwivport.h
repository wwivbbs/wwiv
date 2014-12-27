/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014,WWIV Software Services             */
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

#ifndef __INCLUDED_PLATFORM_INCL1_H__
#define __INCLUDED_PLATFORM_INCL1_H__

#if defined( __APPLE__ ) && !defined( __unix__ )
#define __unix__
#endif

#if defined ( __unix__ )

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <utime.h>
#define O_BINARY  0
#define O_TEXT    0
#define SH_DENYNO 0
#define SH_DENYWR 0
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#endif // defined ( __unix__ )

#endif // __INCLUDED_PLATFORM_INCL1_H__
