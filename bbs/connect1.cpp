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

// *************************************
// DO NOT EXTRACT STRINGS FROM THIS FILE
// **************************************

#if defined (NET)
#include "vardec.h"
#include "net.h"
#else
#include "wwiv.h"
#endif
#include "WStringUtils.h"


void read_bbs_list();
int system_index( int ts );


void zap_call_out_list()
{
	if (net_networks[GetSession()->GetNetworkNumber()].con)
    {
		BbsFreeMemory( net_networks[GetSession()->GetNetworkNumber()].con );
		net_networks[ GetSession()->GetNetworkNumber() ].con = NULL;
		net_networks[ GetSession()->GetNetworkNumber() ].num_con = 0;
	}
}


void read_call_out_list()
{
	net_call_out_rec *con;

	zap_call_out_list();

	WFile fileCallout( GetSession()->GetNetworkDataDirectory(), CALLOUT_NET );
	if ( fileCallout.Open( WFile::modeBinary | WFile::modeReadOnly ) )
	{
		long lFileLength = fileCallout.GetLength();
		char *ss = NULL;
		if ( ( ss = static_cast<char*>( BbsAllocA( lFileLength + 512 ) ) ) == NULL )
		{
			WWIV_ASSERT( ss != NULL );
            GetApplication()->AbortBBS( true );
		}
		fileCallout.Read( ss, lFileLength );
		fileCallout.Close();
		for (long lPos = 0L; lPos < lFileLength; lPos++)
		{
			if (ss[lPos] == '@')
			{
				++net_networks[GetSession()->GetNetworkNumber()].num_con;
			}
		}
		BbsFreeMemory(ss);
		if ((net_networks[GetSession()->GetNetworkNumber()].con = (net_call_out_rec *)
			BbsAllocA( static_cast<long>( (net_networks[GetSession()->GetNetworkNumber()].num_con + 2) *
			sizeof(net_call_out_rec) ) )) == NULL)
		{
			WWIV_ASSERT(net_networks[GetSession()->GetNetworkNumber()].con != NULL);
			GetApplication()->AbortBBS( true );
		}
		con = net_networks[GetSession()->GetNetworkNumber()].con;
		con--;
		fileCallout.Open( WFile::modeBinary | WFile::modeReadOnly );
		if ( ( ss = static_cast<char*>( BbsAllocA(lFileLength + 512 ) ) ) == NULL )
		{
			WWIV_ASSERT(ss != NULL);
			GetApplication()->AbortBBS( true );
		}
		fileCallout.Read( ss, lFileLength );
		fileCallout.Close();
		long p = 0L;
		while (p < lFileLength)
		{
			while ((p < lFileLength) && (strchr("@%/\"&-=+~!();^|#$_*", ss[p]) == NULL))
			{
				++p;
			}
			if (p < lFileLength)
			{
				switch (ss[p])
				{
				case '@':
					++p;
					con++;
					con->macnum			= 0;
					con->options		= 0;
					con->call_anyway	= 0;
					con->password[0]	= 0;
                    con->sysnum			= wwiv::stringUtils::StringToUnsignedShort(&(ss[p]));
					con->min_hr			= -1;
					con->max_hr			= -1;
					con->times_per_day	= 0;
					con->min_k			= 0;
					con->call_x_days	= 0;
					break;
				case '&':
					con->options |= options_sendback;
					++p;
					break;
				case '-':
					con->options |= options_ATT_night;
					++p;
					break;
				case '_':
					con->options |= options_ppp;
					++p;
					break;
				case '+':
					con->options |= options_no_call;
					++p;
					break;
				case '~':
					con->options |= options_receive_only;
					++p;
					break;
				case '!':
					con->options |= options_once_per_day;
					++p;
                    con->times_per_day = wwiv::stringUtils::StringToUnsignedChar(&(ss[p]));
					if (!con->times_per_day)
					{
						con->times_per_day = 1;
					}
					break;
				case '%':
					++p;
					con->macnum = wwiv::stringUtils::StringToUnsignedChar(&(ss[p]));
					break;
				case '/':
					++p;
					con->call_anyway = wwiv::stringUtils::StringToUnsignedChar(&(ss[p]));
					++p;
					break;
				case '#':
					++p;
					con->call_x_days = wwiv::stringUtils::StringToUnsignedChar(&(ss[p]));
					break;
				case '(':
					++p;
					con->min_hr = wwiv::stringUtils::StringToChar(&(ss[p]));
					break;
				case ')':
					++p;
					con->max_hr = wwiv::stringUtils::StringToChar(&(ss[p]));
					break;
				case '|':
					++p;
					con->min_k = wwiv::stringUtils::StringToUnsignedShort(&(ss[p]));
					if (!con->min_k)
					{
						con->min_k = 0;
					}
					break;
				case ';':
					++p;
					con->options |= options_compress;
					break;
				case '^':
					++p;
					con->options |= options_hslink;
					break;
				case '$':
					++p;
					con->options |= options_force_ac;
					break;
				case '=':
					++p;
					con->options |= options_hide_pend;
					break;
				case '*':
					++p;
					con->options |= options_dial_ten;
					break;
				case '\"':
					{
						++p;
						int i = 0;
						while ( ( i < 19 ) && (ss[p + static_cast<long>( i )] != '\"' ) )
						{
							++i;
						}
						for (int i1 = 0; i1 < i; i1++)
						{
							con->password[i1] = ss[ p + static_cast<long>( i1 ) ];
						}
						con->password[i] = 0;
						p += static_cast<long>( i + 1 );
					}
					break;
				}
			}
		}
		BbsFreeMemory( ss );
	}
}



int bbs_list_net_no = -1;


void zap_bbs_list()
{
	if ( csn )
	{
		BbsFreeMemory( csn );
		csn = NULL;
	}
	if ( csn_index )
	{
		BbsFreeMemory( csn_index );
		csn_index = NULL;
	}
	GetSession()->num_sys_list	= 0;
	bbs_list_net_no			= -1;
}


void read_bbs_list()
{
	zap_bbs_list();

	if (net_sysnum == 0)
	{
		return;
	}
	WFile fileBbsData( GetSession()->GetNetworkDataDirectory(), BBSDATA_NET );
	if ( fileBbsData.Open( WFile::modeBinary | WFile::modeReadOnly ) )
	{
		long lFileLength = fileBbsData.GetLength();
		GetSession()->num_sys_list = static_cast<int>( lFileLength / sizeof( net_system_list_rec ) );
		if ( ( csn = static_cast<net_system_list_rec *>( BbsAllocA( lFileLength + 512L ) ) ) == NULL )
		{
			WWIV_ASSERT( csn != NULL );
			GetApplication()->AbortBBS( true );
		}
		for ( int i = 0; i < GetSession()->num_sys_list; i += 256 )
		{
			fileBbsData.Read( &(csn[i]), 256 * sizeof( net_system_list_rec ) );
		}
		fileBbsData.Close();
	}
	bbs_list_net_no = GetSession()->GetNetworkNumber();
}


void read_bbs_list_index()
{
	zap_bbs_list();

	if ( net_sysnum == 0 )
	{
		return;
	}
	WFile fileBbsData( GetSession()->GetNetworkDataDirectory(), BBSDATA_IND );
	if ( fileBbsData.Open( WFile::modeBinary | WFile::modeReadOnly ) )
	{
		long lFileLength = fileBbsData.GetLength();
		GetSession()->num_sys_list = static_cast<int>( lFileLength / 2 );
		if ( ( csn_index = static_cast<unsigned short *>( BbsAllocA( lFileLength ) ) ) == NULL )
		{
			WWIV_ASSERT( csn_index != NULL );
			GetApplication()->AbortBBS( true );
		}
		fileBbsData.Read( csn_index, lFileLength );
		fileBbsData.Close();
	}
	else
	{
		read_bbs_list();
	}
	bbs_list_net_no = GetSession()->GetNetworkNumber();
}


int system_index( int ts )
{
	if (bbs_list_net_no != GetSession()->GetNetworkNumber())
	{
		read_bbs_list_index();
	}

	if ( csn )
	{
		for ( int i = 0; i < GetSession()->num_sys_list; i++ )
		{
			if ( csn[i].sysnum == ts && csn[i].forsys != 65535 )
			{
				return i;
			}
		}
	}
	else
	{
		for ( int i = 0; i < GetSession()->num_sys_list; i++ )
		{
			if (csn_index[i] == ts)
			{
				return i;
			}
		}
	}
	return -1;
}


bool valid_system( int ts )
{
    return ( system_index( ts ) == -1 ) ? false : true;
}


net_system_list_rec *next_system( int ts )
{
	static net_system_list_rec csne;
	int nIndex = system_index( ts );

	if ( nIndex == -1 )
	{
		return NULL;
	}
	else if ( csn )
	{
		return ( ( net_system_list_rec * ) & ( csn[nIndex] ) );
	}
	else
	{
		WFile fileBbsData( GetSession()->GetNetworkDataDirectory(), BBSDATA_NET );
		fileBbsData.Open( WFile::modeBinary | WFile::modeReadOnly );
		fileBbsData.Seek( sizeof( net_system_list_rec ) * static_cast<long>( nIndex ), WFile::seekBegin );
		fileBbsData.Read( &csne, sizeof( net_system_list_rec ) );
		fileBbsData.Close();
        return ( csne.forsys == 65535 ) ? NULL : ( &csne );
	}
}



void zap_contacts()
{
	if ( net_networks[GetSession()->GetNetworkNumber()].ncn )
    {
		BbsFreeMemory(net_networks[GetSession()->GetNetworkNumber()].ncn);
		net_networks[GetSession()->GetNetworkNumber()].ncn = NULL;
		net_networks[GetSession()->GetNetworkNumber()].num_ncn = 0;
	}
}

void read_contacts()
{
	zap_contacts();

	WFile fileContact( GetSession()->GetNetworkDataDirectory(), CONTACT_NET );
	if ( fileContact.Open( WFile::modeBinary | WFile::modeReadOnly ) )
	{
		long lFileLength = fileContact.GetLength();
		net_networks[GetSession()->GetNetworkNumber()].num_ncn = static_cast< short >( lFileLength / sizeof( net_contact_rec ) );
		if ((net_networks[GetSession()->GetNetworkNumber()].ncn =
			static_cast<net_contact_rec *>( BbsAllocA((net_networks[GetSession()->GetNetworkNumber()].num_ncn + 2) * sizeof(net_contact_rec)))) == NULL)
		{
			WWIV_ASSERT(net_networks[GetSession()->GetNetworkNumber()].ncn != NULL);
			GetApplication()->AbortBBS( true );
		}
		fileContact.Seek( 0L, WFile::seekBegin );
		fileContact.Read( net_networks[GetSession()->GetNetworkNumber()].ncn, net_networks[GetSession()->GetNetworkNumber()].num_ncn * sizeof( net_contact_rec ) );
		fileContact.Close();
	}
}


void set_net_num( int nNetworkNumber )
{
	if ( nNetworkNumber >= 0 && nNetworkNumber < GetSession()->GetMaxNetworkNumber() )
	{
		GetSession()->SetNetworkNumber( nNetworkNumber );
		//GetSession()->pszNetworkName = net_networks[GetSession()->GetNetworkNumber()].name;
		//GetSession()->pszNetworkDataDir = net_networks[GetSession()->GetNetworkNumber()].dir;
		net_sysnum = net_networks[GetSession()->GetNetworkNumber()].sysnum;
		GetSession()->SetCurrentNetworkType( net_networks[ GetSession()->GetNetworkNumber() ].type );
		snprintf( GetApplication()->m_szEnvironVarWwivNetworkNumber, sizeof( GetApplication()->m_szEnvironVarWwivNetworkNumber ), "WWIV_NET=%ld", GetSession()->GetNetworkNumber() );
	}
}

