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
#else
#error "port this!"
#endif
}

std::string WWIV_GetOSVersion() {
	std::string buffer;
#if defined (_WIN32)
	OSVERSIONINFO os;

	os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (!GetVersionEx(&os))
	{
		return std::string("WIN32");
	}
	switch (os.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_NT:
		if (os.dwMajorVersion == 5)	{
			switch (os.dwMinorVersion) {
			case 0:
				wwiv::stringUtils::FormatString(buffer, "Windows 2000 %s", os.szCSDVersion);
				break;
			case 1:
				wwiv::stringUtils::FormatString(buffer, "Windows XP %s", os.szCSDVersion);
				break;
			case 2:
				wwiv::stringUtils::FormatString(buffer, "Windows Server 2003 %s", os.szCSDVersion);
				break;
			}
		}
		else if (os.dwMajorVersion == 6)	{
			switch (os.dwMinorVersion) {
			case 0:
				wwiv::stringUtils::FormatString(buffer, "Windows Vista %s", os.szCSDVersion);
				break;
			case 1:
				wwiv::stringUtils::FormatString(buffer, "Windows 7 %s", os.szCSDVersion);
				break;
			case 2:
				wwiv::stringUtils::FormatString(buffer, "Windows 8 %s", os.szCSDVersion);
				break;
			case 3:
				wwiv::stringUtils::FormatString(buffer, "Windows 8.1 %s", os.szCSDVersion);
				break;
			default:
				wwiv::stringUtils::FormatString(buffer, "Windows NT %ld%c%ld %s", os.dwMajorVersion, '.', os.dwMinorVersion, os.szCSDVersion);
				break;
			}
		}
		break;
	default:
		wwiv::stringUtils::FormatString(buffer, "WIN32 Compatable OS v%d%c%d", os.dwMajorVersion, '.', os.dwMinorVersion);
	}

#elif defined (__OS2__)
	//
	// TODO Add OS/2 version information code here..
	buffer = std::string("OS/2");
#elif defined ( __linux__ )
	buffer = std::string("Linux");
	WFile info("/proc/sys/kernel", "osrelease");

	if(info.Exists()) {
		info.Open();
		if(info.IsOpen()) {
			char szBuffer2[100];
			info.Read(&szBuffer2, 100);
			info.Close();
			buffer = "Linux " + szBuffer2;
		}
	}
#elif defined ( __APPLE__ )
	wwiv::stringUtils::FormatString(buffer, "%s %s", GetOSNameString(), GetMacVersionString() );
#elif defined ( __unix__ )
	// TODO Add Linux version information code here..
	buffer = "UNIX";
#else
#error "What's the platform here???"
#endif
	return buffer;
}
