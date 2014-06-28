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
#ifndef __INCLUDED_IFCNS_H__
#define __INCLUDED_IFCNS_H__

#include <string>
#include "ivardec.h"
#include "bbs/vardec.h"

/* File: init.cpp */

int verify_dir(char *typeDir, char *dirName);

/* File: init1.cpp */

int number_userrecs();
void read_qscn(unsigned int un, unsigned long *qscn, int stayopen);
void write_qscn(unsigned int un, unsigned long *qscn, int stayopen);
int exist(const char *s);
void save_status();
bool read_status();
int save_config();
void create_text(const char *fn);
void read_user(unsigned int un, userrec *u);
void write_user(unsigned int un, userrec *u);
void exit_init(int level);

/* File: init2.cpp */

void trimstr(char *s);
void trimstrpath(char *s);
void up_subs_dirs();

// ** OSD CODE **
/* File: filesupp.cpp */
void WWIV_ChangeDirTo(const char *s);

#ifndef _WIN32
/* File: stringstuff.cpp */
char *strupr(char *s);

/* File: utility2.cpp */
long filelength(int handle);
#endif  // _WIN32

#endif  // __INCLUDED_IFCNS_H__
