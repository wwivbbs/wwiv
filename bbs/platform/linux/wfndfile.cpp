/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2006, WWIV Software Services             */
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


//////////////////////////////////////////////////////////////////////////////
//
// Local function prototypes
//
char *getdir_from_file(const char *pszFileName);
int fname_ok(const struct dirent *ent);

int dos_flag = false;
char fileSpec[256];
long lTypeMask;

#define TYPE_DIRECTORY	4
#define TYPE_FILE	8

//////////////////////////////////////////////////////////////////////////////
//
// Local functions
//
char *getdir_from_file(const char *pszFileName)
{
   static char s[256];
   int i;

   s[0] = 0;
   for (i=strlen(pszFileName); i>-1; i--)
   {
      if (pszFileName[i] == '/')
      {
        strcpy(s, pszFileName);
        s[i] = 0;
        break;
      }
   }

   if (s[0])
     return(s);

   return("./");
}

int fname_ok( const struct dirent *ent )
{
    int ok, i;
    char f[13], *ptr=NULL, s3[13];
    const char *s1 = fileSpec;
    const char *s2 = ent->d_name;

    if( wwiv::stringUtils::IsEquals( s2, "." ) ||
        wwiv::stringUtils::IsEquals( s2, ".." ) )
    {
        return 0;
    }

    if( lTypeMask )
    {
        if( ent->d_type & TYPE_DIRECTORY && !( lTypeMask & WFINDFILE_DIRS ) )
        {
            return 0;
        }
        else if( ent->d_type & TYPE_FILE && !( lTypeMask & WFINDFILE_FILES ) )
        {
            return 0;
        }
    }

    ok=1;

    f[0] = '\0';
    if (dos_flag)
    {
        if (strlen(s2) > 12 || s2[0] == '.')
        {
            return 0;
        }

        strcpy(s3, s2);
        if (strlen(s3) < 12 && (ptr=strchr(s3, '.')) != NULL)
        {
            *ptr = '\0';
            strcpy(f, s3);
            for (i=strlen(f); i<8; i++)
            {
                f[i] = '?';
            }

            f[i] = '.';
            f[++i] = '\0';
            strcat(f, ptr+1 );

            if ( strlen(f) < 12 )
            {
                memset( &f[strlen(f)], 32, 12-strlen( f ) );
            }

            f[12] = '\0';
        }
        else
        {
            if ( ptr == NULL )
            {
                return 0;
            }
        }
    }

    if (!dos_flag)
    {
        for (i=0; i<255 && ok; i++)
        {
            if ((s1[i]!=s2[i]) && (s1[i]!='?') && (s2[i]!='?'))
            {
                ok=0;
            }
        }
    }
    else
    {
        for (i=0; i<12 && ok; i++)
        {
            if (s1[i]!=f[i])
            {
                if (s1[i] != '?')
                {
                    ok=0;
                }
            }
        }
    }

    return ok;
}


//////////////////////////////////////////////////////////////////////////////
//
// Exported functions
//
//

bool WFindFile::open(const char * pszFileSpec, UINunsigned intT32 nTypeMask)
{
	char szFileName[MAX_PATH];
	char szDirectoryName[MAX_PATH];
	unsigned int i, f, laststar;

	__open(pszFileSpec, nTypeMask);
	dos_flag = 0;

	strcpy(szDirectoryName, getdir_from_file(pszFileSpec));
	strcpy(szFileName, stripfn(pszFileSpec));

    if ( wwiv::stringUtils::IsEquals( szFileName, "*.*" )  ||
         wwiv::stringUtils::IsEquals( szFileName, "*" ) )
	{
		memset( szFileSpec, '?', 255 );
	}
	else
	{
		f = laststar = szFileSpec[0] = 0;
		for (i=0; i<strlen(szFileName); i++)
		{
			if (szFileName[i] == '*')
			{
				if (i < 8)
				{
					if (strchr(szFileName, '.') != NULL)
					{
						dos_flag = 1;
						memset(&szFileSpec[f], '?', 8-i);
						f += 8-i;
						while (szFileName[++i] != '.')
							;
						i--;

						continue;
					}
				}

				do
				{
					if (szFileName[i] == '.' && i < strlen(szFileName))
					{
						szFileSpec[f++] = '.';
						break;
					}
					szFileSpec[f++] = '?';
				} while (++i < 255);

			}
			else
			{
				szFileSpec[f++] = szFileName[i];
			}
		}

		if (strchr(szFileSpec, '.') == NULL && f < 255)
		{
			memset(&szFileSpec[f], '?', 255-f);
		}

		if (strstr(szFileName, ".*") != NULL && dos_flag)
		{
			memset(&szFileSpec[9], '?', 3);
		}

		if (strlen(szFileSpec) < 255 && !dos_flag)
		{
			memset(&szFileSpec[f], 32, 255-f);
		}
	}
	szFileSpec[255] = 0;

	if (dos_flag)
	{
		szFileSpec[12] = 0;
	}

	strcpy(fileSpec, szFileSpec);

	nMatches = scandir(szDirectoryName, &entries, fname_ok, alphasort);
	if (nMatches < 0)
	{
		std::cout << "could not open dir '" << szDirectoryName << "'\r\n";
		perror("scandir");
		return false;
	}
	nCurrentEntry = 0;

	next();
	nCurrentEntry = 0;
	return ( nMatches > 0 );
}

bool WFindFile::next()
{
    if(nCurrentEntry >= nMatches)
    {
        return( false );
    }
    struct dirent *entry = entries[nCurrentEntry++];

    strcpy(szFileName, entry->d_name);
    lFileSize = entry->d_reclen;
    return( true );
}

bool WFindFile::close()
{
	__close();
	return true;
}

bool WFindFile::IsDirectory()
{
    if(nCurrentEntry >= nMatches)
    {
        return( false );
    }

    struct dirent *entry = entries[nCurrentEntry];
    return (entry->d_type & TYPE_DIRECTORY);
}

bool WFindFile::IsFile()
{
    if(nCurrentEntry >= nMatches)
    {
        return( false );
    }

    struct dirent *entry = entries[nCurrentEntry];
    return (entry->d_type & TYPE_FILE);
}

