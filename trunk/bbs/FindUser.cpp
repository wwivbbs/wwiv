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


#include "incl1.h"
#include "WConstants.h"
#include "filenames.h"
#include "WFile.h"
#include "WUser.h"
#include "WStringUtils.h"
#include "vars.h"
#include "bbs.h"

// TODO - Remove this and finduser, finduser1, ISR, DSR, and add_add
#include "fcns.h"

//
// Returns user number
//      or 0 if user not found
//      or special value
//
// This function will remove the special characters from arround the pszSearchString that are
// used by the network and remote, etc.
//
// Special values:
//
//  -1      = NEW USER
//  -2      = WWIVnet
//  -3      = Remote Command
//  -4      = Unknown Special Login
//
int finduser( char *pszSearchString )
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
    if ( strncmp( pszSearchString, "!=@", 3 ) == 0 )
    {
        char* ss = pszSearchString + strlen( pszSearchString ) - 3;
        if ( wwiv::stringUtils::IsEquals( ss, "@=!" ) )
        {
            strcpy( pszSearchString, pszSearchString + 3 );
            pszSearchString[ strlen( pszSearchString ) - 3 ] = '\0';
            return -4;
        }
    }
    int nUserNumber = atoi( pszSearchString );
    if ( nUserNumber > 0 )
    {
        GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
        if ( user.isUserDeleted() )
        {
	    //printf( "DEBUG: User %s is deleted!\r\n", user.GetName() );
            return 0;
        }
        return nUserNumber;
    }
    GetApplication()->GetStatusManager()->Read();
    smalrec *sr = ( smalrec * ) bsearch( ( void * ) pszSearchString,
                    ( void * ) smallist,
                    ( size_t ) status.users,
                    ( size_t ) sizeof( smalrec ),
                    ( int (*) ( const void *, const void * ) ) strcmp );

    if ( sr == 0L )
    {
        return 0;
    }
    else
    {
        GetApplication()->GetUserManager()->ReadUser( &user, sr->number );
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

    char szUserNamePart[ 255 ];
    strcpy( szUserNamePart, pszSearchString );
    for ( int i = 0; szUserNamePart[i] != 0; i++ )
    {
        szUserNamePart[i] = upcase( szUserNamePart[i] );
    }
    GetApplication()->GetStatusManager()->Read();
    for ( int i1 = 0; i1 < status.users; i1++ )
    {
        if ( strstr( reinterpret_cast<char*>( smallist[i1].name ), szUserNamePart) != NULL )
        {
            int nCurrentUserNum = smallist[i1].number;
            WUser user;
            GetApplication()->GetUserManager()->ReadUser( &user, nCurrentUserNum );
            GetSession()->bout << "|#5Do you mean " << user.GetUserNameAndNumber( nCurrentUserNum ) << " (Y/N/Q)? ";
            char ch = ynq();
            if ( ch == 'Y' )
            {
                return nCurrentUserNum;
            }
            if ( ch == 'Q' )
            {
                return 0;
            }
        }
    }
    return 0;
}
