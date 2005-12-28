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


#include "incl1.h"
#include "WConstants.h"
#include "filenames.h"
#include "WFile.h"
#include "WUser.h"
#include "WSession.h"
#include "WStringUtils.h"
#include "vars.h"
#include "bbs.h"
#include <memory>
#include "WStatus.h"

// TODO - Remove this and finduser, finduser1, ISR, DSR, and add_add
#include "fcns.h"

//
// Returns user number
//      or 0 if user not found
//      or special value
//
// This function will remove the special characters from arround the searchString that are
// used by the network and remote, etc.
//
// Special values:
//
//  -1      = NEW USER
//  -2      = WWIVnet
//  -3      = Remote Command
//  -4      = Unknown Special Login
//
int finduser( const std::string searchString )
{
    WUser user;

    guest_user = false;
    GetApplication()->GetUserManager()->SetUserWritesAllowed( true );
    if ( searchString == "NEW" )
    {
        return -1;
    }
    if ( searchString == "!-@NETWORK@-!" )
    {
        return -2;
    }
    if ( searchString == "!-@REMOTE@-!" )
    {
        return -3;
    }
    int nUserNumber = atoi( searchString.c_str() );
    if ( nUserNumber > 0 )
    {
        GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
        if ( user.IsUserDeleted() )
        {
	        //printf( "DEBUG: User %s is deleted!\r\n", user.GetName() );
            return 0;
        }
        return nUserNumber;
    }
    smalrec *sr = ( smalrec * ) bsearch( ( const void * ) searchString.c_str(),
                    ( const void * ) smallist,
                    ( size_t ) GetApplication()->GetStatusManager()->GetUserCount(),
                    ( size_t ) sizeof( smalrec ),
                    ( int (*) ( const void *, const void * ) ) wwiv::stringUtils::StringCompare );

    if ( sr == 0L )
    {
        return 0;
    }
    else
    {
        GetApplication()->GetUserManager()->ReadUser( &user, sr->number );
        if ( user.IsUserDeleted() )
        {
            return 0;
        }
        else
        {
            if ( wwiv::stringUtils::IsEqualsIgnoreCase( user.GetName(), "GUEST" ) )
            {
                guest_user = true;
                GetApplication()->GetUserManager()->SetUserWritesAllowed( false );
            }
            return sr->number;
        }
    }
}


// Takes user name/handle as parameter, and returns user number, if found,
// else returns 0.
int finduser1( const std::string searchString )
{
    if ( searchString.empty() )
    {
        return 0;
    }
    int nFindUserNum = finduser( searchString );
    if ( nFindUserNum > 0 )
    {
        return nFindUserNum;
    }

    char szUserNamePart[ 255 ];
    strcpy( szUserNamePart, searchString.c_str() );
    for ( int i = 0; szUserNamePart[i] != 0; i++ )
    {
        szUserNamePart[i] = upcase( szUserNamePart[i] );
    }
    for ( int i1 = 0; i1 < GetApplication()->GetStatusManager()->GetUserCount(); i1++ )
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
