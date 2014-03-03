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

bool WWIV_GetOSVersion(	char * pszOSVersionString,
                        int nBufferSize,
                        bool bFullVersion) {

#if defined (_WIN32)

	OSVERSIONINFO os;
	char szBuffer[200];

	os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (!GetVersionEx(&os))
	{
		strcpy(pszOSVersionString, "WIN32");
		return false;
	}

	switch (os.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_WINDOWS:
		if ((os.dwMajorVersion == 4) && (os.dwMinorVersion == 0))
		{
			snprintf(szBuffer, sizeof(szBuffer), "Windows 95");
		}
		else if (os.dwMajorVersion == 4 && os.dwMinorVersion == 10)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Windows 98");
			if (os.szCSDVersion[1] == 'A')
			{
				strcat(szBuffer, "SE ");
			}
		}
		else if (os.dwMajorVersion == 4 && os.dwMinorVersion == 90)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Windows ME");
		}
		else
		{
			snprintf(szBuffer, sizeof(szBuffer), "Windows %ld%c%ld", os.dwMajorVersion, '.', os.dwMinorVersion);
		}
		break;
	case VER_PLATFORM_WIN32_NT:
		if (os.dwMajorVersion == 5)
		{
			switch (os.dwMinorVersion)
			{
			case 0:
				snprintf(szBuffer, sizeof(szBuffer), "Windows 2000 %s", (bFullVersion ? os.szCSDVersion : ""));
				break;
			case 1:
				snprintf(szBuffer, sizeof(szBuffer), "Windows XP %s", (bFullVersion ? os.szCSDVersion : ""));
				break;
			case 2:
				snprintf(szBuffer, sizeof(szBuffer), "Windows Server 2003 %s", (bFullVersion ? os.szCSDVersion : ""));
				break;
			}
		}
		else if (os.dwMajorVersion == 6 && os.dwMinorVersion == 0)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Windows Vista %s", (bFullVersion ? os.szCSDVersion : ""));
		}
		else if (os.dwMajorVersion == 6 && os.dwMinorVersion == 1)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Windows 7 %s", (bFullVersion ? os.szCSDVersion : ""));
		}
		else if (os.dwMajorVersion == 6 && os.dwMinorVersion == 2)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Windows 8 %s", (bFullVersion ? os.szCSDVersion : ""));
		}
		else if (os.dwMajorVersion == 6 && os.dwMinorVersion == 3)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Windows 8.1 %s", (bFullVersion ? os.szCSDVersion : ""));
		}
		else
		{
			snprintf(szBuffer, sizeof(szBuffer), "Windows NT %ld%c%ld %s", os.dwMajorVersion, '.', os.dwMinorVersion, (bFullVersion ? os.szCSDVersion : ""));
		}
		break;
	case VER_PLATFORM_WIN32s:
		// Don't know why we need this, we won't run here.
		snprintf(szBuffer, sizeof(szBuffer), "WIN32s on Windows 3.1");
		break;
	default:
		snprintf(szBuffer, sizeof(szBuffer), "WIN32 Compatable OS v%d%c%d", os.dwMajorVersion, '.', os.dwMinorVersion);
	}

	if (nBufferSize < wwiv::stringUtils::GetStringLength(szBuffer))
	{
		szBuffer[nBufferSize - 1] = '\0';
	}
	strcpy(pszOSVersionString, szBuffer);

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
