/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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
#include "WStringUtils.h"


void show_files( const char *pszFileName, const char *pszDirectoryName )
// Displays list of files matching filespec pszFileName in directory pszDirectoryName.
{
    char s[255];
    char drive[MAX_DRIVE], direc[MAX_DIR], file[MAX_FNAME], ext[MAX_EXT];

    char c = ( okansi() ) ? '\xCD' : '=';
    nl();

    _splitpath(pszDirectoryName, drive, direc, file, ext);
    sprintf(s, "|#7[|B1|15 FileSpec: %s    Dir: %s%s |B0|#7]", strupr(stripfn(pszFileName)), drive, direc);
    int i = ( sess->thisuser.GetScreenChars() - 1 ) / 2 - strlen(stripcolors(s)) / 2;
    sess->bout << "|#7" << charstr( i, c ) << s;
    i = sess->thisuser.GetScreenChars() - 1 - i - strlen(stripcolors(s));
    sess->bout << "|#7" << charstr( i, c );

    char szFullPathName[ MAX_PATH ];
    sprintf( szFullPathName, "%s%s", pszDirectoryName, strupr( stripfn(pszFileName ) ) );
    WFindFile fnd;
    bool bFound = fnd.open( szFullPathName, 0 );
    while (bFound)
    {
        strcpy(s, fnd.GetFileName());
        align(s);
        sprintf( szFullPathName, "|#7[|#2%s|#7]|#1 ", s );
        if ( app->localIO->WhereX() > ( sess->thisuser.GetScreenChars() - 15 ) )
        {
            nl();
        }
        sess->bout << szFullPathName;
        bFound = fnd.next();
    }

    nl();
    ansic( 7 );
    sess->bout << charstr( sess->thisuser.GetScreenChars() - 1, c );
    nl( 2 );
}



// Used only by WWIV_make_abs_cmd

char *exts[] =
{
	"",
	".COM",
	".EXE",
	".BAT",
	".BTM",
	".CMD",
	0
};



char *WWIV_make_abs_cmd(char *out)
{
    // TODO Move this into platform specific code
    char s[MAX_PATH], s1[MAX_PATH], s2[MAX_PATH], *ss1;
    char szTempBuf[MAX_PATH];

    char szWWIVHome[MAX_PATH];
    strcpy( szWWIVHome, app->GetHomeDir() );

    strcpy( s1, out );

    if ( s1[1] == ':' )
    {
        if ( s1[2] != '\\' )
        {
            _getdcwd( wwiv::UpperCase<char>(s1[0]) - 'A' + 1, s, MAX_PATH );
            if (s[0])
            {
                sprintf( s1, "%c:\\%s\\%s", s1[0], s, out + 2 );
            }
            else
            {
                sprintf( s1, "%c:\\%s", s1[0], out + 2 );
            }
        }
    }
    else if ( s1[0] == '\\' )
    {
        sprintf( s1, "%c:%s", szWWIVHome[0], out );
    }
    else
    {
        strcpy(s2, s1);
        strtok(s2, " \t");
        if (strchr(s2, '\\'))
        {
            sprintf(s1, "%s%s", app->GetHomeDir(), out);
        }
    }

    char* ss = strchr(s1, ' ');
    if (ss)
    {
        *ss = '\0';
        sprintf(s2, " %s", ss + 1);
    }
    else
    {
        s2[0] = '\0';
    }
    for (int i = 0; exts[i]; i++)
    {
        if (i == 0)
        {
            ss1 = strrchr(s1, '\\');
            if (!ss1)
            {
                ss1 = s1;
            }
            if (strchr(ss1, '.') == 0)
            {
                continue;
            }
        }
        sprintf(s, "%s%s", s1, exts[i]);
        if (s1[1] == ':')
        {
            if (WFile::Exists(s))
            {
                sprintf(out, "%s%s", s, s2);
                goto got_cmd;
            }
        }
        else
        {
            if (WFile::Exists(s))
            {
                sprintf(out, "%s%s%s", app->GetHomeDir(), s, s2);
                goto got_cmd;
            }
            else
            {
                _searchenv(s, "PATH", szTempBuf);
                ss1 = szTempBuf;
                if ((ss1) && (strlen(ss1)>0))
                {
                    sprintf(out, "%s%s", ss1, s2);
                    goto got_cmd;
                }
            }
        }
    }

    sprintf(out, "%s%s%s", app->GetHomeDir(), s1, s2);

got_cmd:
    return out;
}


#define LAST(s)	s[strlen(s)-1]


int WWIV_make_path(char *s)
{
    char drive, current_path[MAX_PATH], current_drive, *p, *flp;

    p = flp = strdup(s);
    _getdcwd(0, current_path, MAX_PATH);
    current_drive = static_cast< char >( *current_path - '@' );
    if (LAST(p) == WWIV_FILE_SEPERATOR_CHAR)
    {
        LAST(p) = 0;
    }
    if (p[1] == ':')
    {
		drive = static_cast< char >( wwiv::UpperCase<char>(p[0]) - 'A' + 1 );
        if (_chdrive(drive))
        {
            chdir(current_path);
            _chdrive(current_drive);
            return -2;
        }
        p += 2;
    }
    if (*p == WWIV_FILE_SEPERATOR_CHAR)
    {
        chdir(WWIV_FILE_SEPERATOR_STRING);
        p++;
    }
    for (; (p = strtok(p, WWIV_FILE_SEPERATOR_STRING)) != 0; p = 0)
    {
        if (chdir(p))
        {
            if (mkdir(p))
            {
                chdir(current_path);
                _chdrive(current_drive);
                return -1;
            }
            chdir(p);
        }
    }
    chdir(current_path);
    if (flp)
    {
        BbsFreeMemory(flp);
    }
    return 0;
}

#if defined (LAST)
#undef LAST
#endif


void WWIV_Delay(unsigned long usec)
{
    Sleep( usec );
}

void WWIV_OutputDebugString( const char *pszString )
{
    ::OutputDebugString( pszString );
}

