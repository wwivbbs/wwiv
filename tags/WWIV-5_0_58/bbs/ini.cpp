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
#include "ini.h"


struct ini_info_t
{
    int num;
    char *buf;
    char **key;
    char **value;
};


static ini_info_t ini_prim, ini_sec;

using namespace wwiv::stringUtils;


// Allocates memory and returns pointer to location containing requested data
// within a file.
static char *mallocin_subsection(const char *pszFileName, long begin, long end)
{
    char* ss = NULL;

    WFile file( pszFileName );
    if ( file.Open( WFile::modeReadOnly | WFile::modeBinary ) )
    {
        ss = static_cast<char *>( bbsmalloc( end - begin + 2 ) );
        if (ss)
        {
            file.Seek( begin, WFile::seekBegin );
            file.Read( ss, ( end - begin + 1 ) );
            ss[(end - begin + 1)] = 0;
        }
        file.Close();
    }
    return ss;
}


// Returns begin and end locations for specified subsection within an INI file.
// If subsection not found then *begin and *end are both set to -1L.
static void find_subsection_area(const char *pszFileName, const char *ssn, long *begin, long *end)
{
    char s[255], szTempHeader[81], *ss;

    *begin = *end = -1L;
    snprintf( szTempHeader, sizeof( szTempHeader ), "[%s]", ssn );

    FILE *f = fsh_open( pszFileName, "rt");
    if (!f)
    {
        return;
    }

    long pos = 0L;

    while (fgets(s, sizeof(s) - 1, f) != NULL)
    {
        // Get rid of CR/LF at end
        ss = strchr(s, '\n');
        if (ss)
        {
            *ss = 0;
        }

        // Get rid of trailing/leading spaces
        StringTrim(s);

        // A comment or blank line?
        if ( s[0] && s[0] != ';' )
        {
            // Is it a subsection header?
            if ((strlen(s) > 2) && (s[0] == '[') && (s[strlen(s) - 1] == ']'))
            {
                // Does it match requested subsection name (ssn)?
                if ( strnicmp(&s[0], &szTempHeader[0], strlen( szTempHeader ) ) == 0 )
                {
                    if (*begin == -1L)
                    {
                        *begin = ftell(f);
                    }
                }
                else
                {
                    if (*begin != -1L)
                    {
                        if (*end == -1L)
                        {
                            *end = pos - 1;
                            break;
                        }
                    }
                }
            }
        }
        // Update file position pointer
        pos = ftell(f);
    }

    // Mark end as end of the file if not already found
    if ( *begin != -1L && *end == -1L )
    {
        *end = ftell(f) - 1;
    }

    fsh_close(f);
}


// Reads a subsection from specified .INI file, the subsection being specified
// by *hdr. Returns a ptr to the subsection data if found and memory is
// available, else returns NULL.
static char *read_ini_file(const char *pszFileName, const char *hdr)
{
    // Header must be "valid", and file must exist
    if (!hdr || !(*hdr) || strlen(hdr) < 1 || !WFile::Exists( pszFileName ) )
    {
        return NULL;
    }

    // Get area to read in
    long beginloc = -1L, endloc = -1L;
    find_subsection_area(pszFileName, hdr, &beginloc, &endloc);

    // Validate
    if (beginloc >= endloc)
    {
        return NULL;
    }

    // Allocate pointer to hold data
    char* ss = mallocin_subsection(pszFileName, beginloc, endloc);
    return ss;
}


static void parse_ini_file(char *pBuffer, ini_info_t * info)
{
    char *ss1, *ss, *ss2;
    unsigned int count = 0;

    memset(info, 0, sizeof(ini_info_t));
    info->buf = pBuffer;

    // first, count # key-value pairs
    unsigned int i1 = strlen( pBuffer );
    char* tempb = static_cast<char *>( bbsmalloc( i1 + 20 ) );
    if (!tempb)
    {
        return;
    }

    memmove(tempb, pBuffer, i1);
    tempb[i1] = 0;

    for (ss = strtok(tempb, "\r\n"); ss; ss = strtok(NULL, "\r\n"))
    {
        StringTrim(ss);
        if ( ss[0] == 0 || ss[0] == ';' )
        {
            continue;
        }

        ss1 = strchr(ss, '=');
        if (ss1)
        {
            *ss1 = 0;
            StringTrim(ss);
            if (*ss)
            {
                count++;
            }
        }
    }

    BbsFreeMemory(tempb);

    if (!count)
    {
        return;
    }

    // now, allocate space for key-value pairs
    info->key = static_cast<char **>( bbsmalloc( count * sizeof( char * ) ) );
    if (!info->key)
    {
        return;
    }
    info->value = static_cast<char **>( bbsmalloc( count * sizeof( char * ) ) );
    if (!info->value)
    {
        BbsFreeMemory(info->key);
        info->key = NULL;
        return;
    }
    // go through and add in key-value pairs
    for (ss = strtok(pBuffer, "\r\n"); ss; ss = strtok(NULL, "\r\n"))
    {
        StringTrim(ss);
        if ( ss[0] == 0 || ss[0] == ';' )
        {
            continue;
        }

        ss1 = strchr(ss, '=');
        if (ss1)
        {
            *ss1 = 0;
            StringTrim(ss);
            if (*ss)
            {
                ss1++;
                ss2 = ss1;
                while ((ss2[0]) && (ss2[1]) && ((ss2 = strchr(ss2 + 1, ';')) != NULL))
                {
                    if (isspace(*(ss2 - 1)))
                    {
                        *ss2 = 0;
                        break;
                    }
                }
                StringTrim(ss1);
                info->key[info->num] = ss;
                info->value[info->num] = ss1;
                info->num++;
            }
        }
    }
}


// Frees up any allocated ini files
void ini_done()
{
    if (ini_prim.buf)
    {
        BbsFreeMemory(ini_prim.buf);
        if (ini_prim.key)
        {
            BbsFreeMemory(ini_prim.key);
        }
        if (ini_prim.value)
        {
            BbsFreeMemory(ini_prim.value);
        }
    }
    memset(&ini_prim, 0, sizeof(ini_prim));
    if (ini_sec.buf)
    {
        BbsFreeMemory(ini_sec.buf);
        if (ini_sec.key)
        {
            BbsFreeMemory(ini_sec.key);
        }
        if (ini_sec.value)
        {
            BbsFreeMemory(ini_sec.value);
        }
    }
    memset(&ini_sec, 0, sizeof(ini_sec));
}


// Reads in some ini files
bool ini_init(const char *pszFileName, const char *prim, const char *sec)
{
    char szIniFile[MAX_PATH+MAX_FNAME];
    snprintf( szIniFile, sizeof( szIniFile ), "%s%s", app->GetHomeDir(), pszFileName );

    // first, zap anything there currently
    ini_done();

    // read in primary info
    char* pBuffer = read_ini_file(szIniFile, prim);

    if ( pBuffer )
    {
        // parse the data
        parse_ini_file(pBuffer, &ini_prim);

        // read in secondary file
        pBuffer = read_ini_file(szIniFile, sec);
        if (pBuffer)
        {
            parse_ini_file(pBuffer, &ini_sec);
        }

    }
    else
    {
        // read in the secondary info, as the primary one
        pBuffer = read_ini_file( szIniFile, sec );
        if ( pBuffer )
        {
            parse_ini_file( pBuffer, &ini_prim );
        }
    }

    return ( ini_prim.buf ) ? true : false;
}




// Reads a specified value from INI file data (contained in *inidata). The
// name of the value to read is contained in *value_name. If such a name
// doesn't exist in this INI file subsection, then *val is NULL, else *val
// will be set to the string value of that value name. If *val has been set
// to something, then this function returns 1, else it returns 0.
char *ini_get( const char *key, int index, char *index1 )
{
    char key1[81], key2[81];
    ini_info_t *info;

    if ( !ini_prim.buf || !key || !( *key ) )
    {
        return NULL;
    }

    if ( index == -1 )
    {
        strcpy( key1, key );
    }
    else
    {
        snprintf( key1, sizeof( key1 ), "%s[%d]", key, index );
    }

    if ( index1 )
    {
        snprintf( key2, sizeof( key2 ), "%s[%s]", key, index1 );
    }
    else
    {
        key2[0] = '\0';
    }

    // loop through both sets of data and search them, in order
    for ( int i = 0; i <= 1; i++ )
    {
        // get pointer to data area to use
        if ( i == 0 )
        {
            info = &ini_prim;
        }
        else if ( ini_sec.buf )
        {
            info = &ini_sec;
        }
        else
        {
            break;
        }

        // search for it
        for ( int i1 = 0; i1 < info->num; i1++ )
        {
            if ( IsEqualsIgnoreCase( info->key[ i1 ], key1 ) || IsEqualsIgnoreCase( info->key[ i1 ], key2 ) )
            {
                return info->value[ i1 ];
            }
        }
    }


    // nothing found
    return NULL;
}


unsigned long ini_flags(char yes_char, const char *(*func) (int), ini_flags_rec * fs, int num, unsigned long flags)
{
    yes_char = wwiv::UpperCase<char>( yes_char );

    for (int i = 0; i < num; i++)
    {
        const char* ss = func(fs[i].strnum);
        if (ss)
        {
            const char* ss1 = ini_get(ss, -1, NULL);
            if (ss1)
            {
                if (wwiv::UpperCase<char>(*ss1) == yes_char)
                {
                    if (fs[i].sense)
                    {
                        flags &= ~fs[i].value;
                    }
                    else
                    {
                        flags |= fs[i].value;
                    }
                }
                else
                {
                    if (fs[i].sense)
                    {
                        flags |= fs[i].value;
                    }
                    else
                    {
                        flags &= ~fs[i].value;
                    }
                }
            }
        }
    }

    return flags;
}
