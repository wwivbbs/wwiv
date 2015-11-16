/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#ifndef __INCLUDED_DATETIME_H__
#define __INCLUDED_DATETIME_H__

#include <ctime>
#include <string>
#include <time.h>	// For time_t

char *dateFromTimeTForLog(time_t t);
char *dateFromTimeT(time_t t);
char *date();
char *fulldate();
char *times();
time_t date_to_daten(const char *datet);
void filedate(const char *fpath, char *rtn);
double timer();
long timer1();
bool isleap(int nYear);
int dow();
char *ctim(double d);
std::string ctim2(double d);
int years_old(int nMonth, int nDay, int nYear);


#endif // __INCLUDED_DATETIME_H__
