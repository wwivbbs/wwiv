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

#ifndef __INCLUDED_PLATFORM_INCL1_H__
#define __INCLUDED_PLATFORM_INCL1_H__

////////////////////////////////////////////////////////////////////////////////
// WIN32 Specific section
//

#if defined (_WIN32)

#if defined (_MSC_VER)
#pragma component(browser, off)
#endif // defined (_MSC_VER)

#define WWIV_FILE_SEPERATOR_CHAR	'\\'
#define WWIV_FILE_SEPERATOR_STRING	"\\"

#if defined (__GNUC__)
#define timezone _timezone
#endif // __GNUC__

#define WWIV_VSNPRINTF _vsnprintf
#define snprintf _snprintf

#endif	 // _WIN32


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
#define O_BINARY	0
#define O_TEXT		0
#define SH_DENYNO	0
#define SH_DENYWR	0
#define SOCKET		int
#define HANDLE		int
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#define SOCKADDR_IN	struct sockaddr_in
#define LPSOCKADDR	(struct sockaddr *)
#define INVALID_SOCKET	-1
#define SOCKET_ERROR	-1
#define ADDR_ANY	INADDR_ANY
#define SOCKADDR	struct sockaddr
#define WWIV_FILE_SEPERATOR_CHAR	'/'
#define WWIV_FILE_SEPERATOR_STRING	"/"

#define _tzset(s)	tzset(s)
#define _putenv(s)	putenv(s)
#define _getcwd(a,b)	getcwd(a,b)
#define mkdir(x)	mkdir(x, S_IRWXU | S_IRWXG)

#define WWIV_VSNPRINTF vsnprintf

#endif // defined ( __unix__ )


#endif // __INCLUDED_PLATFORM_INCL1_H__
