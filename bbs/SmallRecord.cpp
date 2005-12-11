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
#include "WStringUtils.h"
#include "vars.h"
#include "bbs.h"
#include <iostream>

// TODO - Remove this and finduser, finduser1, ISR, DSR, and add_add
#include "fcns.h"



//
// Inserts a record into NAMES.LST
//
void InsertSmallRecord(int nUserNumber, const char *pszName)
{
    smalrec sr;
    int cp = 0;
    GetApplication()->GetStatusManager()->Lock();
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
        GetApplication()->AbortBBS();
    }
    ++status.users;
    ++status.filechange[filechange_names];
    namesList.Write( smallist, ( sizeof( smalrec ) * status.users ) );
    namesList.Close();
    GetApplication()->GetStatusManager()->Write();
}


//
// Deletes a record from NAMES.LST (DeleteSmallRec)
//

void DeleteSmallRecord( const char *pszName )
{
    int cp = 0;
    GetApplication()->GetStatusManager()->Lock();
    while ( cp < status.users && !wwiv::stringUtils::IsEquals( pszName, reinterpret_cast<char*>( smallist[cp].name ) ) )
    {
        ++cp;
    }
    if ( !wwiv::stringUtils::IsEquals( pszName, reinterpret_cast<char*>( smallist[cp].name ) ) )
    {
        GetApplication()->GetStatusManager()->Write();
        sysoplogfi( false, "%s NOT ABLE TO BE DELETED#*#*#*#*#*#*#*#", pszName );
        sysoplog( "#*#*#*# Run //resetf to fix it", false );
        return;
    }
    for ( int i = cp; i < status.users - 1; i++ )
    {
        smallist[i] = smallist[i + 1];
    }
    WFile namesList( syscfg.datadir, NAMES_LST );
    if ( !namesList.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeTruncate ) )
    {
        std::cout << namesList.GetFullPathName() << " COULD NOT BE CREATED" << std::endl;
        GetApplication()->AbortBBS();
    }
    --status.users;
    ++status.filechange[filechange_names];
    namesList.Write( smallist, ( sizeof( smalrec ) * status.users ) );
    namesList.Close();
    GetApplication()->GetStatusManager()->Write();
}
