/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2005, WWIV Software Services             */
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

#ifndef __INCLUDED_WUTIL_H__
#define __INCLUDED_WUTIL_H__

void WWIV_Sound(int nFreq, int nDly);

int WWIV_GetRandomNumber(int nMaxValue);

// Gets the OS Version Number
// pszOSVersion is a string containing the full version information
// nBufferSize is the size of the buffer pointed to by pszOSVersionString
// returns true on success, false on error
//
bool WWIV_GetOSVersion(char * pszOSVersionString, int nBufferSize, bool bFullVersion);


#endif // __INCLUDED_WUTIL_H__
