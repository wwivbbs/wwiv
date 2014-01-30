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

#include "wwiv.h"

void WWIV_Sound(int nFreq, int nDly) {
#ifdef _WIN32
	::Beep(nFreq, nDly);
#endif
}


int WWIV_GetRandomNumber(int nMaxValue) {
#if defined (_WIN32)

	int num = rand();
	float rn = static_cast<float>( num/RAND_MAX );
	return static_cast<int>((nMaxValue-1) * rn);

#elif defined ( __unix__ ) || defined ( __APPLE__ )

	int num = random();
	float rn = static_cast<float>( num/RAND_MAX );
	return static_cast<int>((nMaxValue-1) * rn);

#elif defined (__OS2__)

#error "port this!"

#else

#error "port this!"


#endif
}


#if defined(_WIN32) && !defined(_USING_V110_SDK71_) && ( _MSC_VER >= 1800 )
#include <VersionHelpers.h>
#endif

bool WWIV_GetOSVersion(	char * pszOSVersionString,
                        int nBufferSize,
                        bool bFullVersion) {
#if defined (_WIN32) && !defined(_USING_V110_SDK71_) && ( _MSC_VER >= 1800 )
	if (IsWindows8Point1OrGreater()) {
		strcpy(pszOSVersionString, "Windows 8.1+");
		return false;
	} else if (IsWindows8OrGreater()) {
		strcpy(pszOSVersionString, "Windows 8");
		return false;
	} else if (IsWindows7OrGreater()) {
		strcpy(pszOSVersionString, "Windows 7");
		return false;
	} else if (IsWindowsVistaOrGreater()) {
		strcpy(pszOSVersionString, "Windows Vista");
		return false;
	} else if (IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WS03), LOBYTE(_WIN32_WINNT_WS03), 0)) {
		strcpy(pszOSVersionString, "Windows Server 2003");
		return false;
	} else if (IsWindowsXPOrGreater()) {
		strcpy(pszOSVersionString, "Windows XP");
		return false;
	} else if (IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_NT4), LOBYTE(_WIN32_WINNT_NT4), 0)) {
		strcpy(pszOSVersionString, "Windows NT");
		return false;
	} else {
		// couldn't figure it out, give up
		strcpy(pszOSVersionString, "Windows");
		return false;
	}
#elif defined (_WIN32)
	strcpy(pszOSVersionString, "Windows");
	return false;
#elif defined (__OS2__)

	//
	// TODO Add OS/2 version information code here..
	//
	strcpy(pszOSVersionString, "OS/2");


#elif defined ( __linux__ )

	char szBuffer[200];
	strcpy(szBuffer, "Linux");
	WFile info("/proc/sys/kernel", "osrelease");

	if(info.Exists()) {
		info.Open();
		if(info.IsOpen()) {
			char szBuffer2[100];
			info.Read(&szBuffer2, 100);
			info.Close();
			snprintf( szBuffer, sizeof( szBuffer ), "Linux %s", szBuffer2 );
		}
	}
	strcpy(pszOSVersionString, szBuffer);

#elif defined ( __APPLE__ )

	strcpy( pszOSVersionString, GetOSNameString() );
	strcat( pszOSVersionString, " " );
	strcat( pszOSVersionString, GetMacVersionString() );

#elif defined ( __unix__ )

	//
	// TODO Add Linux version information code here..
	//
	strcpy(pszOSVersionString, "UNIX");

#else
#error "What's the platform here???"
#endif
}
