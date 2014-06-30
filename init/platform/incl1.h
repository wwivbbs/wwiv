/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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

#ifndef __INCLUDED_PLATFORM_INCL1_H__
#define __INCLUDED_PLATFORM_INCL1_H__

////////////////////////////////////////////////////////////////////////////////
// WIN32 Specific section
//

#if defined (_WIN32)

#define WWIV_FILE_SEPERATOR_CHAR  '\\'
#define WWIV_FILE_SEPERATOR_STRING  "\\"
#define MAX_FNAME _MAX_FNAME

#endif   // _WIN32

////////////////////////////////////////////////////////////////////////////////
// UNIX Common Section.
//

#if defined ( __unix__ ) || defined ( __APPLE__ )

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
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#define MAX_EXT   256
#define MAX_DIR   256
#define MAX_FNAME 256
#define MAX_DRIVE 256
#define WWIV_FILE_SEPERATOR_CHAR  '/'
#define WWIV_FILE_SEPERATOR_STRING  "/"

#define mkdir(x)  mkdir(x, S_IRWXU | S_IRWXG)

#endif // defined ( __unix__ )

#ifdef __APPLE__
#define WWIV_STRICMP(a, b) strcasecmp(a, b)
#define stricmp strcasecmp
#define strnicmp strncasecmp

#define _close close
#define _write write
#define _lseek lseek
#define _read read
#define _open open

#endif  // __APPLE__

#endif // __INCLUDED_PLATFORM_INCL1_H__
