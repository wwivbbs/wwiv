/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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


void add_phone_number( int usernum, const char *phone )
{
	if (strstr(phone, "000-"))
	{
		return;
	}

    WFile phoneFile( syscfg.datadir, PHONENUM_DAT );
    if ( !phoneFile.Open( WFile::modeReadWrite | WFile::modeAppend | WFile::modeBinary | WFile::modeCreateFile,
                          WFile::shareUnknown, WFile::permReadWrite ) )
	{
		return;
	}

	phonerec p;
    p.usernum = static_cast<short>( usernum );
	strcpy( reinterpret_cast<char*>( p.phone ), phone );
    phoneFile.Write( &p, sizeof( phonerec ) );
    phoneFile.Close();
}


void delete_phone_number( int usernum, const char *phone )
{
    WFile phoneFile( syscfg.datadir, PHONENUM_DAT );
    if ( !phoneFile.Open( WFile::modeReadWrite | WFile::modeBinary ) )
	{
		return;
	}
    long lFileSize = phoneFile.GetLength();
	int nNumRecords = static_cast<int>( lFileSize / sizeof( phonerec ) );
	phonerec *p = static_cast<phonerec *>( BbsAllocA( lFileSize ) );
	WWIV_ASSERT(p);
	if (p == NULL)
	{
		return;
	}
    phoneFile.Read( p, lFileSize );
    phoneFile.Close();
    int i;
	for (i = 0; i < nNumRecords; i++)
	{
		if ( p[i].usernum == usernum &&
             wwiv::stringUtils::IsEquals( reinterpret_cast<char*>( p[i].phone ), phone ) )
		{
			break;
		}
	}
	if (i < nNumRecords)
	{
		for (int i1 = i; i1 < nNumRecords; i1++)
		{
			p[i1] = p[i1 + 1];
		}
		--nNumRecords;
        phoneFile.Delete();
        phoneFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite );
        phoneFile.Write( p, static_cast<long>( nNumRecords * sizeof( phonerec ) ) );
        phoneFile.Close();
	}
	BbsFreeMemory( p );
}


int find_phone_number(const char *phone)
{
    WFile phoneFile( syscfg.datadir, PHONENUM_DAT );
    if ( !phoneFile.Open( WFile::modeReadWrite | WFile::modeBinary ) )
	{
		return 0;
	}
    long lFileSize = phoneFile.GetLength();
	int nNumRecords = static_cast<int>( lFileSize / sizeof( phonerec ) );
	phonerec *p = static_cast<phonerec *>( BbsAllocA( lFileSize ) );
	WWIV_ASSERT(p);
	if (p == NULL)
	{
		return 0;
	}
    phoneFile.Read( p, lFileSize );
    phoneFile.Close();
    int i = 0;
	for (i = 0; i < nNumRecords; i++)
	{
		if ( wwiv::stringUtils::IsEquals( reinterpret_cast<char*>( p[i].phone ), phone ) )
		{
        	WUser user;
            GetApplication()->GetUserManager()->ReadUser( &user, p[i].usernum );
            if ( !user.IsUserDeleted() )
			{
				break;
			}
		}
	}
	BbsFreeMemory(p);
	if (i < nNumRecords)
	{
		return p[i].usernum;
	}
	return 0;
}

