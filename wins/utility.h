#if !defined( __UTILITY_H__INCLUDED__ )
#define __UTILITY_H__INCLUDED__
/*
 * Copyright 2001,2004 Frank Reid
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */ 

//
// Defines
//
#define NUL '\0'


//
// MACROS
//
#define LAST(s)	s[strlen(s)-1]


//
// Function prototypes.
//
void giveup_timeslice(void);
char *stripspace(char *str);
void output(const char *fmt,...);
int log_it(int display, char *fmt,...);
void backline(void);
int do_spawn(const char *cl);
void cd_to(const char *s);
void get_dir(char *s, bool be);
char *trimstr1(char *s);
char *make_abs_path( char *checkdir, const char* maindir );
bool exist(const char *s);
bool exists(const char *s);
int make_path(char *s);
int copyfile(char *infn, char *outfn);
int copyfile(char *infn, char *outfn, bool bOverwrite);
void do_exit(int exitlevel);
double WOSD_FreeSpaceForDriveLetter(int nDrive);
char *trim(char *str);
void go_back(int from, int to);
int isleap(unsigned yr);
static unsigned months_to_days(unsigned month);
int jdate(unsigned yr, unsigned mo, unsigned day);
char *stristr(char *string, char *pattern);
int count_lines(char *buf);
char *rip( char *s );
char *strrep(char *str, char oldstring, char newstring);
bool SetFileToCurrentTime(LPCTSTR pszFileName);

int WhereX();
int WhereY();


//
// SEH Stack Support
//
#define SEH_STACK_SIZE 10
void SEH_Init();
void SEH_PushStack( char *name );
void SEH_DumpStack();
void SEH_PushLineLabel( char *name );

#if defined( __WWIV__SEH__ )
#define SEH_INIT() SEH_Init();
#define SEH_PUSH(s) SEH_PushStack( s );
#define SEH_DUMP()	SEH_DumpStack();
#define SEH_LINE(s)	SEH_PushLineLabel( s );
#else
#define SEH_INIT();
#define SEH_PUSH(s);
#define SEH_DUMP();
#define SEH_LINE(s);
#endif

#endif // __UTILITY_H__INCLUDED__
