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


extern unsigned char *translate_letters[];


int number_userrecs()
{
    WFile userList( syscfg.datadir, USER_LST );
    if ( userList.Open( WFile::modeReadOnly | WFile::modeBinary ) )
    {
        int nNumRecords = ( static_cast<int>( userList.GetLength() / syscfg.userreclen) - 1 );
        return nNumRecords;
    }
    return 0;
}


/*
* Checks status of given userrec to see if conferencing is turned on.
*/
bool okconf( WUser *pUser )
{
    if ( g_flags & g_flag_disable_conf )
    {
        return false;
    }

    return pUser->hasStatusFlag( WUser::conference );
}


char *WUser::nam( int nUserNumber ) const
{
    static char s_szNamBuffer[81];
    bool f = true;
    unsigned int p = 0;
    for (p = 0; p < strlen( this->GetName() ); p++)
    {
        if (f)
        {
            unsigned char* ss = reinterpret_cast<unsigned char*>( strchr( reinterpret_cast<char*>( translate_letters[1] ), data.name[p] ) );
            if (ss)
            {
                f = false;
            }
            s_szNamBuffer[p] = data.name[p];
        }
        else
        {
            char* ss = strchr( reinterpret_cast<char*>( translate_letters[1] ), data.name[p] );
            if (ss)
            {
                s_szNamBuffer[p] = locase(data.name[p]);
            }
            else
            {
                if (((data.name[p] >= ' ') && (data.name[p] <= '/')) && (data.name[p] != 39))
                {
                    f = true;
                }
                s_szNamBuffer[p] = data.name[p];
            }
        }
    }
    s_szNamBuffer[p++] = SPACE;
    s_szNamBuffer[p++] = '#';
    sprintf( &s_szNamBuffer[p], "%d", nUserNumber );
    return s_szNamBuffer;
}


char *WUser::nam1( int nUserNumber, int nSystemNumber ) const
{
    static char s_szNamBuffer[ 255 ];

    strcpy( s_szNamBuffer, nam( nUserNumber ) );
    if ( nSystemNumber )
    {
        char szBuffer[10];
        sprintf( szBuffer, " @%u", nSystemNumber );
        strcat( s_szNamBuffer, szBuffer );
    }
    return s_szNamBuffer;
}


//
// Inserts a record into NAMES.LST
//
void InsertSmallRecord(int nUserNumber, const char *pszName)
{
    smalrec sr;
    int cp = 0;
    app->statusMgr->Lock();
    while ( cp < status.users &&
            wwiv::stringUtils::StringCompare( pszName, reinterpret_cast<char*>( smallist[cp].name ) ) > 0 )
    {
        ++cp;
    }
    for ( int i = status.users; i > cp; i-- )
    {
        smallist[i] = smallist[i - 1];
    }
    strcpy( reinterpret_cast<char*>( sr.name ), pszName );
    sr.number = static_cast<unsigned short>( nUserNumber );
    smallist[cp] = sr;
    WFile namesList( syscfg.datadir, NAMES_LST );
    if ( !namesList.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeTruncate ) )
    {
        std::cout << namesList.GetFullPathName() << " NOT FOUND" << std::endl;
        app->AbortBBS();
    }
    ++status.users;
    ++status.filechange[filechange_names];
    namesList.Write( smallist, ( sizeof( smalrec ) * status.users ) );
    namesList.Close();
    app->statusMgr->Write();
}


//
// Deletes a record from NAMES.LST (DeleteSmallRec)
//

void DeleteSmallRecord(const char *pszName)
{
    int cp = 0;
    app->statusMgr->Lock();
    while ( cp < status.users && !wwiv::stringUtils::IsEquals( pszName, reinterpret_cast<char*>( smallist[cp].name ) ) )
    {
        ++cp;
    }
    if ( !wwiv::stringUtils::IsEquals( pszName, reinterpret_cast<char*>( smallist[cp].name ) ) )
    {
        app->statusMgr->Write();
        sysoplogfi( false, "%s NOT ABLE TO BE DELETED#*#*#*#*#*#*#*#", pszName);
        sysoplog("#*#*#*# Run //resetf to fix it", false);
        return;
    }
    for (int i = cp; i < status.users - 1; i++)
    {
        smallist[i] = smallist[i + 1];
    }
    WFile namesList( syscfg.datadir, NAMES_LST );
    if ( !namesList.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeTruncate ) )
    {
        std::cout << namesList.GetFullPathName() << " COULD NOT BE CREATED" << std::endl;
        app->AbortBBS();
    }
    --status.users;
    ++status.filechange[filechange_names];
    namesList.Write( smallist, ( sizeof( smalrec ) * status.users ) );
    namesList.Close();
    app->statusMgr->Write();
}


//
// Returns user number
//      or 0 if user not found
//      or special value
//
// Special values:
//
//  -1      = NEW USER
//  -2      = WWIVnet
//  -3      = Remote Command
//  -4      = Unknown Special Login
//
int finduser(char *pszSearchString)
{
    WUser user;

    guest_user = false;
    if ( wwiv::stringUtils::IsEquals( pszSearchString, "NEW" ) )
    {
        return -1;
    }
    if ( wwiv::stringUtils::IsEquals( pszSearchString, "!-@NETWORK@-!" ) )
    {
        return -2;
    }
    if ( wwiv::stringUtils::IsEquals( pszSearchString, "!-@REMOTE@-!" ) )
    {
        return -3;
    }
    if (strncmp(pszSearchString, "!=@", 3) == 0)
    {
        char* ss = pszSearchString + strlen(pszSearchString) - 3;
        if ( wwiv::stringUtils::IsEquals( ss, "@=!" ) )
        {
            strcpy(pszSearchString, pszSearchString + 3);
            pszSearchString[strlen(pszSearchString) - 3] = 0;
            return -4;
        }
    }
    int nUserNumber = atoi(pszSearchString);
    if ( nUserNumber > 0 )
    {
        app->userManager->ReadUser( &user, nUserNumber );
        if ( user.isUserDeleted() )
        {
            return 0;
        }
        return nUserNumber;
    }
    app->statusMgr->Read();
    smalrec *sr = (smalrec *) bsearch((void *) pszSearchString,
                    (void *) smallist,
                    (size_t) status.users,
                    (size_t) sizeof(smalrec),
                    (int (*) (const void *, const void *)) strcmp);

    if (sr == 0L)
    {
        return 0;
    }
    else
    {
        app->userManager->ReadUser( &user, sr->number );
        if ( user.isUserDeleted() )
        {
            return 0;
        }
        else
        {
            if ( wwiv::stringUtils::IsEqualsIgnoreCase( user.GetName(), "GUEST" ) )
            {
                guest_user = true;
            }
            return sr->number;
        }
    }
}


// Takes user name/handle as parameter, and returns user number, if found,
// else returns 0.
int finduser1(const char *pszSearchString)
{
    if ( pszSearchString[0] == '\0' )
    {
        return 0;
    }
    int nFindUserNum = finduser( const_cast<char*>( pszSearchString ) );
    if ( nFindUserNum > 0 )
    {
        return nFindUserNum;
    }

    char szUserNamePart[81];
    strcpy( szUserNamePart, pszSearchString );
    for (int i = 0; szUserNamePart[i] != 0; i++)
    {
        szUserNamePart[i] = upcase(szUserNamePart[i]);
    }
    app->statusMgr->Read();
    for ( int i1 = 0; i1 < status.users; i1++ )
    {
        if ( strstr( reinterpret_cast<char*>( smallist[i1].name ), szUserNamePart) != NULL )
        {
            int nCurrentUserNum = smallist[i1].number;
            WUser user;
            app->userManager->ReadUser( &user, nCurrentUserNum );
            sess->bout << "|#5Do you mean " << user.GetUserNameAndNumber( nCurrentUserNum ) << " (Y/N/Q)? ";
            char ch = ynq();
            if (ch == 'Y')
            {
                return nCurrentUserNum;
            }
            if (ch == 'Q')
            {
                return 0;
            }
        }
    }
    return 0;
}


void add_ass( int nNumPoints, const char *pszReason )
{
    sysoplog(  "***" );
    sysoplogf( "*** ASS-PTS: %d, Reason: [%s]", nNumPoints, pszReason );
    sess->thisuser.SetAssPoints( sess->thisuser.GetAssPoints() + static_cast<unsigned short>( nNumPoints ) );
}


