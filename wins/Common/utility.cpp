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

#include "pppproj.h"


#define WWIV_FILE_SEPERATOR_CHAR '\\'
#define WWIV_FILE_SEPERATOR_STRING "\\"

typedef BOOL (WINAPI *P_GDFSE)(LPCTSTR, PULARGE_INTEGER, 
                                  PULARGE_INTEGER, PULARGE_INTEGER);


extern char net_data[_MAX_PATH];


void giveup_timeslice(void)
{
	SEH_PUSH("giveup_timeslice4()");
	Sleep( 100 );
	Sleep( 0 );
    return;
}


char *stripspace(char *str)
{
	char *obuf, *nbuf;
	SEH_PUSH( "stripspace()" );
	
	if (str) 
	{
		for ( obuf = str, nbuf = str; *obuf; ++obuf ) 
		{
			if ( !isspace( *obuf ) )
			{
				*nbuf++ = *obuf;
			}
		}
		*nbuf = NULL;
	}
	return str;
}

void output(const char *fmt,...)
{
	va_list v;
	char s[ 513 ];
	SEH_PUSH("output()");

	if ( fmt && *fmt )
	{
		va_start(v, fmt);
		vsprintf(s, fmt, v);
		va_end(v);
		printf(s);
	}
}

void log_it( bool display, char *fmt, ... )
{
	va_list v;
	char s[255], szFileName[_MAX_PATH];
	FILE *fp = NULL;
	SEH_PUSH("log_it()");

	if ( fmt && *fmt )
	{
		va_start(v, fmt);
		vsprintf(s, fmt, v);
		va_end(v);

		if ( net_data[0] == '\0' && !display )
		{
			printf( "\n ! can not log a message, net_data is not set!!\n\n" );
			if ( s && *s )
			{
				printf(s);
			}
		}

		sprintf(szFileName, "%sNEWS.LOG", net_data);
		if ((fp = fsh_open(szFileName, "at")) == NULL) 
		{
			output("\n ! Error accessing %s.", szFileName);
			return;
		}
		if ( s && *s )
		{
			fputs(s, fp);
		}
		if (fp != NULL)
		{
			fclose(fp);
		}
		if ( display && s && *s )
		{
			printf(s);
		}
	}
}


void backline(void)
{
	SEH_PUSH("backline()");
	output(" "); for (int i = WhereX(); i > 0; i--) output("\b \b");
	output( " \r" );
}


int do_spawn(const char *cl)
{
	int i, i1, l;
	char *ss1, *ss[30];
	SEH_PUSH("do_spawn()");
	
	ss1 = _strdup(cl);
	ss[0] = ss1;
	i = 1;
	l = strlen(ss1);
	for (i1 = 1; i1 < l; i1++) 
	{
		if (ss1[i1] == 32) 
		{
			ss1[i1] = 0;
			ss[i++] = &(ss1[i1 + 1]);
		}
	}
	ss[i] = NULL;
#ifdef MEGA_DEBUG_LOG_INFO
	output( "\nEXEC: " );
	output( cl );
	output( "\n" );
#endif // #ifdef MEGA_DEBUG_LOG_INFO

	i = (_spawnvpe(P_WAIT, ss[0], ss, environ) & 0x00ff);
	if (ss1 != NULL)
	{
		free(ss1);
	}
	ss1 = NULL;
	return i;
}

void cd_to( const char *s )
{
    char s1[81];
    strcpy(s1, s);
	SEH_PUSH("cd_to()");

    int i = strlen(s1) - 1;
    int db = (s1[i] == '\\');
    if ( i == 0 )
    {
        db = 0;
    }
    if ( i == 2 && s1[1] == ':' )
    {
        db = 0;
    }
    if (db)
    {
        s1[i] = 0;
    }
    _chdir( s1 );
    if ( s[1] == ':' ) 
    {
        int drive = toupper(s[0]) - 'A' + 1;
        _chdrive(drive);	// FIX, On Win32, _chdrive is 'A' = 1, etc..
        if (s[2] == 0)
        {
            _chdir("\\");
        }
    }
}


void get_dir( char *s, bool be )
{
	SEH_PUSH("get_dir()");
    strcpy(s, "X:\\");
    s[0] = ( char ) ( 'A' + ( char ) (_getdrive() - 1) );
    _getdcwd(0, &s[0], 161);
    if ( be ) 
    {
        if ( s[strlen(s) - 1] != '\\' )
        {
            strcat(s, "\\");
        }
    }
}


char *trimstr1(char *s)
{
	SEH_PUSH("trimstr1()");
	if (s == NULL || *s == NULL )
	{
		return "";
	}

    // find real end of it 
    int i = strlen(s);
    while ((i > 0) && (isspace(s[i - 1])))
    {
        --i;
    }
    
    // find real beginning 
    int i1 = 0;
    while ((i1 < i) && (isspace(s[i1])))
    {
        ++i1;
    }
    
    // knock spaces off the length 
    i -= i1;
    
    // move over the desired subsection 
    memmove(s, s + i1, i);
    
    // ensure null-terminated 
    s[i] = 0;
    
    return (s);
}


char *make_abs_path(char *checkdir, const char* maindir)
{
	SEH_PUSH("make_abs_path()");
	char newdir[_MAX_PATH];
	
	if ((strlen(checkdir) < 3) || (checkdir[1] != ':') || (checkdir[2] != '\\')) 
	{
		cd_to(maindir);
		cd_to(checkdir);
		if (LAST(checkdir) == '\\')
		{
			get_dir(newdir, true);
		}
		else
		{
			get_dir(newdir, false);
		}
		cd_to(maindir);
		strcpy(checkdir, newdir);
	}
	return checkdir;
}

bool exist(const char *s)
{
	SEH_PUSH("exist()");
	return (_access(s, 0) == -1) ? false : true;
}


bool exists(const char *s)
{
	SEH_PUSH("exists()");
	struct _finddata_t ff;
	long hFind = _findfirst( s, &ff );
	bool bResult = ( hFind != -1 );
	_findclose( hFind );
	return bResult;
}


int make_path(char *s)
{
	SEH_PUSH("make_path");
    char current_path[MAX_PATH], *p, *flp;

    p = flp = _strdup(s);
    _getdcwd(0, current_path, MAX_PATH);
    int current_drive = toupper(*current_path) - '@';
    if (LAST(p) == WWIV_FILE_SEPERATOR_CHAR)
	{
        LAST(p) = 0;
	}
    if (p[1] == ':') 
	{
        int drive = toupper(p[0]) - 'A' + 1;
        if (_chdrive(drive)) 
		{
            _chdir(current_path);
            _chdrive(current_drive);
            return -2;  
        }
        p += 2;
    }
    if ( *p == WWIV_FILE_SEPERATOR_CHAR ) 
	{
        _chdir( WWIV_FILE_SEPERATOR_STRING );
        p++;
    }
    for (; (p = strtok(p, WWIV_FILE_SEPERATOR_STRING)) != 0; p = 0) 
	{
        if (_chdir(p)) 
		{
            if (_mkdir(p)) 
			{
                _chdir(current_path);
                _chdrive(current_drive);
                return -1;
            }
            _chdir(p);
        }   
    }
    _chdir(current_path);
    if (flp)
	{
        free(flp);
	}
    return 0;
}

// returns 1 on success, 0 on failure
int copyfile(char *infn, char *outfn)
{
	SEH_PUSH("copyfile(2)");
	return copyfile( infn, outfn, true );
}


int copyfile(char *infn, char *outfn, bool bOverwrite)
{
	SEH_PUSH("copyfile(3)");
	return CopyFile( infn, outfn, bOverwrite ? TRUE : FALSE ) ? 1 : 0;
}


void do_exit(int exitlevel)
{
	SEH_PUSH("do_exit()");
	Sleep(2000);
	exit(2 - exitlevel);
}

/**
 * Returns the free disk space for a drive letter, where nDrive is the drive number
 * 1 = 'A', 2 = 'B', etc.
 *
 * @param nDrive The drive number to get the free disk space for.
 */
double WOSD_FreeSpaceForDriveLetter(int nDrive)
{
    unsigned __int64 i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;
	DWORD dwSectPerClust, dwBytesPerSect, dwFreeClusters, dwTotalClusters;
	P_GDFSE pGetDiskFreeSpaceEx = NULL;
	BOOL fResult = FALSE;

	pGetDiskFreeSpaceEx = (P_GDFSE)GetProcAddress ( GetModuleHandle ( "kernel32.dll" ),	"GetDiskFreeSpaceExA" );
    char s[] = "X:\\";
    s[0] = ( char ) 'A' + nDrive - 1;
	char *pszDrive = ( nDrive ) ? s : NULL;

	if ( pGetDiskFreeSpaceEx )
	{
		// win95 osr2+ allows the GetDiskFreeSpaceEx call
		fResult = pGetDiskFreeSpaceEx ( pszDrive,
										(PULARGE_INTEGER)&i64FreeBytesToCaller,
										(PULARGE_INTEGER)&i64TotalBytes,
										(PULARGE_INTEGER)&i64FreeBytes );
        if ( !fResult )
        {
		    fResult = pGetDiskFreeSpaceEx ( NULL,
										    (PULARGE_INTEGER)&i64FreeBytesToCaller,
										    (PULARGE_INTEGER)&i64TotalBytes,
										    (PULARGE_INTEGER)&i64FreeBytes );
        }

		if ( fResult )
		{
			return ( double ) ( __int64 ) ( i64FreeBytesToCaller / 1024 );
		}
	
	} 
	else 
	{
		// this one will artificially cap free space at 2 gigs
		fResult = GetDiskFreeSpace( pszDrive, 
                                    &dwSectPerClust,
                                    &dwBytesPerSect, 
                                    &dwFreeClusters,
                                    &dwTotalClusters);
        if ( !fResult )
        {
		    fResult = GetDiskFreeSpace( NULL, 
                                        &dwSectPerClust,
                                        &dwBytesPerSect, 
                                        &dwFreeClusters,
                                        &dwTotalClusters);
        }

		if ( fResult )
		{
			return ((double)(dwTotalClusters * dwSectPerClust * dwBytesPerSect))/ 1024.0;
		}
	}

	// Nothing worked, just give up.
	return -1.0;
}


char *trim(char *str)
{
	SEH_PUSH("trim()");
	if ( str == NULL )
	{
		return str;
	}
	for ( int i = strlen(str) - 1; ( i >= 0 ) && isspace( str[i] ); str[i--] = '\0' )
		;
	while ( isspace( str[0] ) )
	{
		strcpy( str, str + 1 );
	}
	return str;
}


void go_back( int from, int to )
{
	SEH_PUSH("go_back()");
	for ( int i = from; i > to; i-- ) 
	{
		output( "\b \b" );
	}
}


int isleap( unsigned yr )
{
	SEH_PUSH("isleap()");
	return yr % 400 == 0 || ( yr % 4 == 0 && yr % 100 != 0 );
}

static unsigned months_to_days( unsigned month )
{
	return ( month * 3057 - 3007 ) / 100;
}

int jdate( unsigned yr, unsigned mo, unsigned day )
{
	SEH_PUSH("jdate()");
	int which = day + months_to_days( mo );
	if ( mo > 2 )
	{
		which -= isleap( yr ) ? 1 : 2;
	}
	
	return which;
}


char *stristr( char *string, char *pattern )
{
	SEH_PUSH("stristr()");
	char *pptr, *sptr, *start;
	unsigned int slen, plen;
	
	for ( start = string, pptr = pattern, slen = strlen( string ),
		  plen = strlen( pattern ); slen >= plen; start++, slen-- ) 
	{
		while ( toupper( *start ) != toupper( *pattern ) ) 
		{
			start++;
			slen--;
			if ( slen < plen )
			{
				return NULL;
			}
		}
		sptr = start;
		pptr = pattern;
		while ( toupper( *sptr ) == toupper( *pptr ) ) 
		{
			sptr++;
			pptr++;
			if ( '\0' == *pptr )
			{
				return start;
			}
		}
	}
	return NULL;
}


int count_lines(char *buf)
{
	SEH_PUSH("count_lines()");
	FILE *fp = NULL;
	char szFileName[_MAX_PATH];
	unsigned int nl = 0;
	const int NEWLINE = '\n';
	int c;
	
	strcpy(szFileName, buf);
	if ( ( fp = fsh_open( szFileName, "rt" ) ) == NULL ) 
	{
		output( "\n \xFE Cannot open %s", szFileName );
		return 0;
	}
	while ( ( c = getc( fp ) ) != EOF ) 
	{
		if ( c == NEWLINE )
		{
			++nl;
		}
	}
	if ( fp != NULL )
	{
		fclose( fp );
	}
	return nl;
}


char *rip( char *s )
{
	SEH_PUSH("rip()");
    char *temp;

    if ( ( temp = strchr( s, '\n' ) ) != NULL ) 
	{
		*temp = '\0';
	}
    if ( ( temp = strchr( s, '\r' ) ) != NULL ) 
	{
		*temp = '\0';
	}
    return s;
}


char *strrep(char *str, char oldstring, char newstring)
{
	SEH_PUSH("strrep()");
	for ( int i = 0; str[i]; i++ )
	{
		if ( str[i] == oldstring )
		{
			str[i] = newstring;
		}
	}
	return str;
}


int WhereX()
{
    HANDLE hConOut = GetStdHandle( STD_OUTPUT_HANDLE );
    if ( hConOut == INVALID_HANDLE_VALUE )
    {
        return 0;
    }

    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    GetConsoleScreenBufferInfo(hConOut,&coninfo);
    return coninfo.dwCursorPosition.X + 1;
}

int WhereY()
{
    HANDLE hConOut = GetStdHandle( STD_OUTPUT_HANDLE );
    if ( hConOut == INVALID_HANDLE_VALUE )
    {
        return 0;
    }

    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    GetConsoleScreenBufferInfo(hConOut,&coninfo);
    return coninfo.dwCursorPosition.Y + 1;
}



// SetFileToCurrentTime - sets last write time to current system time
// Return value - TRUE if successful, FALSE otherwise
// hFile  - must be a valid file handle

bool SetFileToCurrentTime(LPCTSTR pszFileName)
{
	SEH_PUSH("SetFileToCurrentTime()");
	// The file must be opened with write _access.
	// That's why GENERIC_WRITE is used for the _access flag.
	HANDLE hFile = CreateFile( pszFileName,
							   GENERIC_WRITE, 
                               FILE_SHARE_READ | FILE_SHARE_WRITE, 
							   NULL, 
							   OPEN_EXISTING, 
							   0, 
							   NULL );

	if ( hFile == INVALID_HANDLE_VALUE )
	{
		return false;
	}

    FILETIME ft;
    SYSTEMTIME st;
    GetSystemTime( &st );              // gets current time
    SystemTimeToFileTime( &st, &ft );  // converts to file time format

	// sets last-write time for file
    bool ret = SetFileTime( hFile, (LPFILETIME) NULL, (LPFILETIME) NULL, &ft ) ? true : false;
	CloseHandle( hFile );
	return ret;
}

//
//
// StackDump SEH Support
//
//

static char *SEH_Stack[ SEH_STACK_SIZE ];


void SEH_Init()
{
	for( int i=0; i < SEH_STACK_SIZE; i++ )
	{
		SEH_Stack[i] = NULL;
	}
}


void SEH_PushStack( char *name )
{
	int top = -1;
	for( int i=0; i < SEH_STACK_SIZE; i++ )
	{
		if ( !SEH_Stack[i] )
		{
			top = i;
			break;
		}
	}

    if ( top > 0 && SEH_Stack[ top - 1] && strcmp( SEH_Stack[ top - 1 ], name ) == 0 )
    {
        // If we've added one, and there is one, and it's the same as we are adding, do nothing.
        return;
    }
	if ( top != -1 )
	{
		SEH_Stack[top] = _strdup( name );
	}
	else
	{
		if ( SEH_Stack[0] )
		{
			free( SEH_Stack[0] );
		}
		for ( int j = SEH_STACK_SIZE - 1; j > 0; j-- )
		{
			SEH_Stack[j-1] = SEH_Stack[j];
		}
		SEH_Stack[SEH_STACK_SIZE-1] = _strdup( name );
	}
}

void SEH_DumpStack()
{
	for ( int i=0; i < SEH_STACK_SIZE; i++ )
	{
		if ( !SEH_Stack[i] )
		{
			break;
		}
		printf( "%s\n", SEH_Stack[i] );
	}
}

void SEH_PushLineLabel( char *name )
{
	UNREFERENCED_PARAMETER( name );
}

