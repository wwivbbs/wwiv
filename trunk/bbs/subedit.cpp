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


//
// Local Function Prototypes
//

void save_subs();
void boarddata(int n, char *s);
void showsubs();
void modify_sub(int n);
void swap_subs(int sub1, int sub2);
void insert_sub(int n);
void delete_sub(int n);

void save_subs()
{
	int nSavedNetNum = GetSession()->GetNetworkNumber();

	for (int nTempNetNum = 0; nTempNetNum < GetSession()->num_subs; nTempNetNum++)
	{
		subboards[nTempNetNum].type = 0;
		subboards[nTempNetNum].age &= 0x7f;
	}

    WFile subsFile( syscfg.datadir, SUBS_DAT );
    if ( !subsFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile | WFile::modeTruncate,
        WFile::shareUnknown, WFile::permReadWrite ) )
	{
        GetSession()->bout << "Error writing subs.dat file." << wwiv::endl;
		pausescr();
	}
	else
	{
		int nNumBytesWritten = subsFile.Write( &subboards[0], GetSession()->num_subs * sizeof( subboardrec ) );
		if ( nNumBytesWritten != ( GetSession()->num_subs * static_cast<int>( sizeof( subboardrec ) ) ) )
		{
            GetSession()->bout << "Error writing subs.dat file ( 2 )." << wwiv::endl;
			pausescr();
		}
        subsFile.Close();
	}

    char szSubsXtrFileName[ MAX_PATH ];
	sprintf(szSubsXtrFileName, "%s%s", syscfg.datadir, SUBS_XTR);
	WFile::Remove(szSubsXtrFileName);
	FILE* fp = fsh_open(szSubsXtrFileName, "w");
	if (fp)
	{
		for (int i = 0; i < GetSession()->num_subs; i++)
		{
			if (xsubs[i].num_nets)
			{
				fprintf(fp, "!%u\n@%s\n#%lu\n", i, xsubs[i].desc, (xsubs[i].flags & XTRA_MASK));
				for (int i1 = 0; i1 < xsubs[i].num_nets; i1++)
				{
					fprintf(fp, "$%s %s %lu %u %u\n",
						        net_networks[xsubs[i].nets[i1].net_num].name,
						        xsubs[i].nets[i1].stype,
						        xsubs[i].nets[i1].flags,
						        xsubs[i].nets[i1].host,
						        xsubs[i].nets[i1].category);
				}
			}
		}
		fsh_close(fp);
	}
	for ( int nDelNetNum = 0; nDelNetNum < GetSession()->GetMaxNetworkNumber(); nDelNetNum++ )
	{
		set_net_num( nDelNetNum );

		WFile::Remove( GetSession()->GetNetworkDataDirectory(), ALLOW_NET );
		WFile::Remove( GetSession()->GetNetworkDataDirectory(), SUBS_PUB );
		WFile::Remove( GetSession()->GetNetworkDataDirectory(), NNALL_NET );
	}

	set_net_num( nSavedNetNum );
}


void boarddata(int n, char *s)
{
	subboardrec r = subboards[n];
    char x = SPACE;
	if (r.ar != 0)
	{
		for ( char i = 0; i < 16; i++ )
		{
			if ( ( 1 << i ) & r.ar )
			{
				x = 'A' + i;
			}
		}
	}
    char y = 'N';
	switch (r.anony & 0x0f)
	{
    case 0:
		y = 'N';
		break;
    case anony_enable_anony:
		y = 'Y';
		break;
    case anony_enable_dear_abby:
		y = 'D';
		break;
    case anony_force_anony:
		y = 'F';
		break;
    case anony_real_name:
		y = 'R';
		break;
	}
    char k = SPACE;
	if (r.key != 0)
	{
		k = r.key;
	}
	sprintf(s, "|#2%4d |#9%1c  |#1%-37.37s |#2%-8s |#9%-3d %-3d %-2d %-5d %7s",
		n, x, stripcolors(r.name), r.filename, r.readsl, r.postsl, r.age & 0x7f,
		r.maxmsgs, xsubs[n].num_nets ? xsubs[n].nets[0].stype : "");
	x = k;
	x = y;
}


void showsubs()
{
    char szSubString[ 41 ];
	ClearScreen();
	bool abort = false;
	GetSession()->bout << "|#7(|#1Message Areas Editor|#7) Enter Substring: ";
	input( szSubString, 20, true );
	pla("|#2NN   AR Name                                  FN       RSL PSL AG MSGS  SUBTYPE", &abort);
	pla("|#7==== == ------------------------------------- ======== --- === -- ===== -------", &abort);
	for (int i = 0; i < GetSession()->num_subs && !abort; i++)
    {
        char szSubData[ 255 ];
		sprintf(szSubData, "%s %s", subboards[i].name, subboards[i].filename);
		if (stristr(szSubData, szSubString))
        {
			subboards[i].anony &= ~anony_require_sv;
			boarddata( i, szSubData );
			pla( szSubData, &abort );
		}
	}
}

char* GetKey( subboardrec r, char *pszKey )
{
    char szKey[ 21 ];
    if (r.key == 0)
    {
	    strcpy(szKey, "None.");
    }
    else
    {
	    szKey[0] = r.key;
	    szKey[1] = 0;
    }
    strcpy( pszKey, szKey );
    return pszKey;
}


char* GetAnon( subboardrec r, char *pszAnon )
{
    char szAnon[81];
	switch (r.anony & 0x0f)
	{
	case 0:
		strcpy(szAnon, YesNoString( false ));
		break;
	case anony_enable_anony:
		strcpy(szAnon, YesNoString( true ));
		break;
	case anony_enable_dear_abby:
		strcpy(szAnon, "Dear Abby");
		break;
	case anony_force_anony:
		strcpy(szAnon, "Forced");
		break;
	case anony_real_name:
		strcpy(szAnon, "Real Name");
		break;
	default:
		strcpy(szAnon, "Real screwed up");
		break;
	}
    strcpy( pszAnon, szAnon );
    return pszAnon;
}


char* GetAr( subboardrec r, char *pszAr )
{
    char szAr[81];
	strcpy(szAr, "None.");
	if (r.ar != 0)
	{
		for (int i = 0; i < 16; i++)
		{
			if ((1 << i) & r.ar)
			{
				szAr[0] = static_cast<char>( 'A' + i );
			}
		}
		szAr[1] = 0;
	}

    strcpy( pszAr, szAr );
    return pszAr;
}


void DisplayNetInfo( int nSubNum )
{
	if (xsubs[nSubNum].num_nets)
	{
		bprintf("\r\n      %-12.12s %-7.7s %-6.6s  Scrb  %s\r\n",
			"Network", "Type", "Host", " Flags");
    	xtrasubsnetrec *xnp = xsubs[nSubNum].nets;
		for (int i = 0; i < xsubs[nSubNum].num_nets; i++, xnp++)
		{
            char szBuffer[255], szBuffer2[255];
			if (xnp->host == 0)
			{
				strcpy(szBuffer, "<HERE>");
			}
			else
			{
				sprintf(szBuffer, "%u ", xnp->host);
			}
			if (xnp->category)
			{
				sprintf(szBuffer2, "%s(%d)", " Auto-Info", xnp->category);
			}
			else
			{
				strcpy(szBuffer2, " Auto-Info");
			}
			if (xnp->host == 0)
			{
                char szNetFileName[ MAX_PATH ];
				sprintf( szNetFileName, "%sn%s.net", net_networks[xnp->net_num].dir, xnp->stype );
				int num = amount_of_subscribers( szNetFileName );
				bprintf("   %c) %-12.12s %-7.7s %-6.6s  %-4d  %s%s\r\n",
					i + 'a',
					net_networks[xnp->net_num].name,
					xnp->stype,
					szBuffer,
					num,
					(xnp->flags & XTRA_NET_AUTO_ADDDROP) ? " Auto-Req" : "",
					(xnp->flags & XTRA_NET_AUTO_INFO) ? szBuffer2 : "");
			}
			else
			{
				bprintf("   %c) %-12.12s %-7.7s %-6.6s  %s%s\r\n",
					i + 'a',
					net_networks[xnp->net_num].name,
					xnp->stype,
					szBuffer,
					(xnp->flags & XTRA_NET_AUTO_ADDDROP) ? " Auto-Req" : "",
					(xnp->flags & XTRA_NET_AUTO_INFO) ? szBuffer2 : "");
			}
		}
	}
	else
	{
        GetSession()->bout << "Not networked.\r\n";
	}

}


void modify_sub(int n)
{
	subboardrec r = subboards[n];
	bool done = false;
	do
	{
        char szKey[21];
        char szAnon[81];
        char szAr[81];

        ClearScreen();
    	char szSubNum[81];
		sprintf(szSubNum, "%s %d", "|B1|15Editing Message Area #", n);
		bprintf("%-85s", szSubNum);
        ansic ( 0 );
		nl( 2 );
        GetSession()->bout << "|#9A) Name       : |#2" << r.name << wwiv::endl;
		GetSession()->bout << "|#9B) Filename   : |#2" << r.filename << wwiv::endl;
		GetSession()->bout << "|#9C) Key        : |#2" << GetKey( r, szKey ) << wwiv::endl;
		GetSession()->bout << "|#9D) Read SL    : |#2" << static_cast<int>( r.readsl ) << wwiv::endl;
		GetSession()->bout << "|#9E) Post SL    : |#2" << static_cast<int>( r.postsl ) << wwiv::endl;
		GetSession()->bout << "|#9F) Anony      : |#2" << GetAnon( r, szAnon ) << wwiv::endl;
		GetSession()->bout << "|#9G) Min. Age   : |#2" << static_cast<int>( r.age & 0x7f ) << wwiv::endl;
		GetSession()->bout << "|#9H) Max Msgs   : |#2" << r.maxmsgs << wwiv::endl;
		GetSession()->bout << "|#9I) AR         : |#2" << GetAr( r,  szAr ) << wwiv::endl;
		GetSession()->bout << "|#9J) Net info   : |#2";
        DisplayNetInfo( n );

		GetSession()->bout << "|#9K) Storage typ: |#2" << r.storage_type << wwiv::endl;
        GetSession()->bout << "|#9L) Val network: |#2" << YesNoString( ( r.anony & anony_val_net ) ? true : false ) << wwiv::endl;
		GetSession()->bout << "|#9M) Req ANSI   : |#2" << YesNoString( ( r.anony & anony_ansi_only ) ? true : false ) << wwiv::endl;
		GetSession()->bout << "|#9N) Disable tag: |#2" << YesNoString( ( r.anony & anony_no_tag ) ? true : false ) << wwiv::endl;
		GetSession()->bout << "|#9O) Description: |#2" << ( ( xsubs[n].desc[0] ) ? xsubs[n].desc : "None." ) << wwiv::endl;
		nl();
		GetSession()->bout << "|#7(|#2Q|#7=|#1Quit|#7) Which (|#1A|#7-|#1O|#7,|#1[|#7=|#1Prev|#7,|#1]|#7=|#1Next|#7) : ";
		char ch = onek( "QABCDEFGHIJKLMNO[]", true );
		switch ( ch )
		{
		case 'Q':
			done = true;
			break;
		case '[':
			subboards[n] = r;
			if ( --n < 0 )
			{
				n = GetSession()->num_subs - 1;
			}
			r = subboards[n];
			break;
		case ']':
			subboards[n] = r;
			if (++n >= GetSession()->num_subs)
			{
				n = 0;
			}
			r = subboards[n];
			break;
		case 'A':
            {
			    nl();
			    GetSession()->bout << "|#2New name? ";
                char szSubName[ 81 ];
			    Input1(szSubName, r.name, 40, true, MIXED);
			    ansic( 0 );
			    if (szSubName[0])
			    {
				    strcpy(r.name, szSubName);
			    }
            }
			break;
		case 'B':
            {
			    nl();
			    GetSession()->bout << "|#2New filename? ";
                char szSubBaseName[ MAX_PATH ];
			    Input1(szSubBaseName, r.filename, 8, true, FILE_NAME);
			    if ( szSubBaseName[0] != 0 && strchr(szSubBaseName, '.') == 0 )
			    {
                    char szOldSubFileName[MAX_PATH];
				    sprintf( szOldSubFileName, "%s%s.sub", syscfg.datadir, szSubBaseName );
				    if (WFile::Exists(szOldSubFileName))
				    {
					    for (int i = 0; i < GetSession()->num_subs; i++)
					    {
						    if (strnicmp(subboards[i].filename, szSubBaseName, strlen(szSubBaseName)) == 0)
						    {
							    strcpy(szOldSubFileName, subboards[i].name);
							    break;
						    }
					    }
					    nl();
                        GetSession()->bout << "|#6" << szSubBaseName << " already in use for \"" << szOldSubFileName << "\"" << wwiv::endl;
					    nl();
					    GetSession()->bout << "|#5Use anyway? ";
					    if (!yesno())
					    {
						    break;
					    }
				    }
				    sprintf(szOldSubFileName, "%s", r.filename);
				    strcpy(r.filename, szSubBaseName);

                    char szFile1[ MAX_PATH ], szFile2[ MAX_PATH ];
                    sprintf(szFile1, "%s%s.sub", syscfg.datadir, r.filename);
				    sprintf(szFile2, "%s%s.dat", syscfg.msgsdir, r.filename);
				    if ( r.storage_type == 2 && !WFile::Exists( szFile1 ) &&
                         !WFile::Exists( szFile2 ) &&
                         !wwiv::stringUtils::IsEquals( r.filename, "NONAME" ) )
				    {
					    nl();
					    GetSession()->bout << "|#7Rename current data files (.SUB/.DAT)? ";
					    if (yesno())
					    {
						    sprintf( szFile1, "%s%s.sub", syscfg.datadir, szOldSubFileName );
						    sprintf( szFile2, "%s%s.sub", syscfg.datadir, r.filename );
						    WFile::Rename(  szFile1, szFile2 );
						    sprintf( szFile1, "%s%s.dat", syscfg.msgsdir, szOldSubFileName );
						    sprintf( szFile2, "%s%s.dat", syscfg.msgsdir, r.filename );
						    WFile::Rename(  szFile1, szFile2 );
					    }
				    }
			    }
            }
			break;
		case 'C':
            {
			    nl();
			    GetSession()->bout << "|#2New Key (space = none) ? ";
			    char ch2 = onek("@%^&()_=\\|;:'\",` ");
                r.key = (ch2 == SPACE) ? 0 : ch2;
            }
			break;
		case 'D':
			{
				char szDef[5];
				sprintf(szDef, "%d", r.readsl);
				nl();
				GetSession()->bout << "|#2New Read SL? ";
                char szNewSL[ 10 ];
				Input1(szNewSL, szDef, 3, true, UPPER);
				int nNewSL = atoi( szNewSL );
				if ( nNewSL >= 0 && nNewSL < 256 && szNewSL[0] )
				{
					r.readsl = static_cast<unsigned char>( nNewSL );
				}
			} break;
		case 'E':
			{
				char szDef[5];
				sprintf(szDef, "%d", r.postsl);
				nl();
				GetSession()->bout << "|#2New Post SL? ";
                char szNewSL[ 10 ];
				Input1(szNewSL, szDef, 3, true, UPPER);
				int nNewSL = atoi(szNewSL);
				if ( nNewSL >= 0 && nNewSL < 256 && szNewSL[0] )
				{
					r.postsl = static_cast<unsigned char>( nNewSL );
				}
			} break;
		case 'F':
            {
                char szCharString[ 21 ];
			    nl();
			    GetSession()->bout << "|#2New Anony (Y,N,D,F,R) ? ";
			    strcpy(szCharString, "NYDFR");
			    szCharString[0] = YesNoString( false )[0];
			    szCharString[1] = YesNoString( true )[0];
			    char ch2 = onek( szCharString );
			    if (ch2 == YesNoString( false )[0])
			    {
				    ch2 = 0;
			    }
			    else if (ch2 == YesNoString( true )[0])
			    {
				    ch2 = 1;
			    }
			    r.anony &= 0xf0;
			    switch (ch2)
			    {
			    case 0:
				    break;
			    case 1:
				    r.anony |= anony_enable_anony;
				    break;
			    case 'D':
				    r.anony |= anony_enable_dear_abby;
				    break;
			    case 'F':
				    r.anony |= anony_force_anony;
				    break;
			    case 'R':
				    r.anony |= anony_real_name;
				    break;
			    }
            }
			break;
			case 'G':
                {
				    nl();
				    GetSession()->bout << "|#2New Min Age? ";
                    char szAge[ 10 ];
				    input(szAge, 3);
				    int nAge = atoi( szAge );
				    if ( nAge >= 0 && nAge < 128 && szAge[0] )
				    {
					    r.age = static_cast<unsigned char>( ( r.age & 0x80 ) | ( nAge & 0x7f ) );
				    }
                }
				break;
			case 'H':
				{
					char szDef[5];
					sprintf(szDef, "%d", r.maxmsgs);
					nl();
					GetSession()->bout << "|#2New Max Msgs? ";
                    char szMaxMsgs[ 21 ];
					Input1(szMaxMsgs, szDef, 5, true, UPPER);
					int nMaxMsgs = atoi( szMaxMsgs );
					if ( nMaxMsgs > 0 && nMaxMsgs < 16384 && szMaxMsgs[0] )
					{
						r.maxmsgs = static_cast<unsigned short>( nMaxMsgs );
					}
				} break;
			case 'I':
                {
				    nl();
                    GetSession()->bout << "|#2Enter New AR (<SPC>=None) : ";
				    char ch2 = onek( "ABCDEFGHIJKLMNOP ", true );
				    if ( ch2 == SPACE )
				    {
					    r.ar = 0;
				    }
				    else
				    {
					    r.ar = 1 << (ch2 - 'A');
				    }
                }
				break;
			case 'J':
                {
				    subboards[n] = r;
                    char ch2 = 'A';
				    if (xsubs[n].num_nets)
				    {
					    nl();
					    GetSession()->bout << "|#2A)dd, D)elete, or M)odify net reference (Q=Quit)? ";
					    ch2 = onek("QAMD");
				    }

				    if (ch2 == 'A')
				    {
					    sub_xtr_add(n, -1);
					    if ( wwiv::stringUtils::IsEquals( subboards[n].name, "** New WWIV Message Area **" ) )
					    {
						    strncpy( subboards[n].name, xsubs[n].desc, 40 );
					    }
					    if ( wwiv::stringUtils::IsEquals( subboards[n].name, "NONAME" ) )
					    {
                            strncpy(subboards[n].filename, xsubs[n].nets->stype, 8);
					    }
				    }
                    else if ( ch2 == 'D' || ch2 == 'M' )
				    {
					    nl();
					    if (ch2 == 'D')
					    {
                            GetSession()->bout << "|#2Delete which (a-";
					    }
					    else
					    {
                            GetSession()->bout << "|#2Modify which (a-";
					    }
					    bprintf("%c", 'a' + xsubs[n].num_nets - 1);
					    GetSession()->bout << "), <space>=Quit? ";
                        char szCharString[ 81 ];
					    szCharString[0] = ' ';
                        int i;
					    for (i = 0; i < xsubs[n].num_nets; i++)
					    {
						    szCharString[i + 1] = static_cast<char>( 'A' + i );
					    }
					    szCharString[i + 1] = 0;
					    ansic( 0 );
					    char ch3 = onek( szCharString );
					    if (ch3 != ' ')
					    {
						    i = ch3 - 'A';
						    if ( i >= 0 && i < xsubs[n].num_nets )
						    {
							    if (ch2 == 'D')
							    {
								    sub_xtr_del(n, i, 1);
							    }
							    else
							    {
								    sub_xtr_del(n, i, 0);
								    sub_xtr_add(n, i);
							    }
						    }
					    }
				    }
				    r = subboards[n];
                }
				break;
			case 'K':
                {
				    nl();
				    GetSession()->bout << "|#2New Storage Type ( 2 ) ? ";
                    char szStorageType[ 10 ];
				    input(szStorageType, 4);
				    int nStorageType = atoi( szStorageType );
				    if ( szStorageType[0] && nStorageType > 1 && nStorageType <= 2 )
				    {
					    r.storage_type = static_cast<unsigned short>( nStorageType );
				    }
                }
				break;
			case 'L':
				nl();
				GetSession()->bout << "|#5Require sysop validation for network posts? ";
				r.anony &= ~anony_val_net;
				if (yesno())
				{
					r.anony |= anony_val_net;
				}
				break;
			case 'M':
				nl();
				GetSession()->bout << "|#5Require ANSI to read this sub? ";
				r.anony &= ~anony_ansi_only;
				if (yesno())
                {
					r.anony |= anony_ansi_only;
                }
				break;
			case 'N':
				nl();
				GetSession()->bout << "|#5Disable tag lines for this sub? ";
				r.anony &= ~anony_no_tag;
				if (yesno())
                {
					r.anony |= anony_no_tag;
                }
				break;
			case 'O':
                {
				    nl();
                    GetSession()->bout << "|#2Enter new Description : ";
                    char szDescription[ 81 ];
				    Input1(szDescription, xsubs[n].desc, 60, true, MIXED);
				    ansic( 0 );
				    if (szDescription[0])
                    {
					    strcpy(xsubs[n].desc, szDescription);
                    }
				    else
                    {
					    nl();
					    GetSession()->bout << "|#2Delete Description? ";
					    if (yesno())
                        {
						    xsubs[n].desc[0] = 0;
                        }
				    }
                }
				break;
    }
  } while ( !done && !hangup );
  subboards[n] = r;
}


void swap_subs(int sub1, int sub2)
{
	SUBCONF_TYPE sub1conv = (SUBCONF_TYPE) sub1;
	SUBCONF_TYPE sub2conv = (SUBCONF_TYPE) sub2;

	if ( sub1 < 0 || sub1 >= GetSession()->num_subs || sub2 < 0 || sub2 >= GetSession()->num_subs )
    {
		return;
    }

	update_conf(CONF_SUBS, &sub1conv, &sub2conv, CONF_UPDATE_SWAP);

	sub1 = static_cast<int>( sub1conv );
	sub2 = static_cast<int>( sub2conv );

	int nNumUserRecords = GetApplication()->GetUserManager()->GetNumberOfUserRecords();

	unsigned long *pTempQScan = static_cast< unsigned long *>(  BbsAllocA( syscfg.qscn_len ) );
	if (pTempQScan)
    {
		for (int i = 1; i <= nNumUserRecords; i++)
        {
        	int i1, i2;
			read_qscn(i, pTempQScan, true);
			unsigned long *pTempQScan_n = pTempQScan + 1;
			unsigned long *pTempQScan_q = pTempQScan_n + (GetSession()->GetMaxNumberFileAreas() + 31) / 32;
			unsigned long *pTempQScan_p = pTempQScan_q + (GetSession()->GetMaxNumberMessageAreas() + 31) / 32;

			if (pTempQScan_q[sub1 / 32] & (1L << (sub1 % 32)))
            {
				i1 = 1;
            }
			else
            {
				i1 = 0;
            }

            if (pTempQScan_q[sub2 / 32] & (1L << (sub2 % 32)))
            {
				i2 = 1;
            }
			else
            {
				i2 = 0;
            }
			if (i1 + i2 == 1)
            {
				pTempQScan_q[sub1 / 32] ^= (1L << (sub1 % 32));
				pTempQScan_q[sub2 / 32] ^= (1L << (sub2 % 32));
			}
			unsigned long tl = pTempQScan_p[sub1];
			pTempQScan_p[sub1] = pTempQScan_p[sub2];
			pTempQScan_p[sub2] = tl;

			write_qscn(i, pTempQScan, true);
		}
		close_qscn();
		BbsFreeMemory(pTempQScan);
	}

    subboardrec sbt     = subboards[sub1];
	subboards[sub1]     = subboards[sub2];
	subboards[sub2]     = sbt;

	unsigned long sdt   = GetSession()->m_SubDateCache[sub1];
	GetSession()->m_SubDateCache[sub1]     = GetSession()->m_SubDateCache[sub2];
	GetSession()->m_SubDateCache[sub2]     = sdt;

	xtrasubsrec xst     = xsubs[sub1];
	xsubs[sub1]         = xsubs[sub2];
	xsubs[sub2]         = xst;

	save_subs();
}


void insert_sub(int n)
{
	subboardrec r;
	int i, i1, i2;
	unsigned long *pTempQScan_n, *pTempQScan_q, *pTempQScan_p, m1, m2, m3;
	SUBCONF_TYPE nconv = (SUBCONF_TYPE) n;

	if ( n < 0 || n > GetSession()->num_subs )
    {
		return;
    }

	update_conf(CONF_SUBS, &nconv, NULL, CONF_UPDATE_INSERT);

	n = static_cast<int>( nconv );

	for (i = GetSession()->num_subs - 1; i >= n; i--)
    {
		subboards[i + 1] = subboards[i];
		GetSession()->m_SubDateCache[i + 1] = GetSession()->m_SubDateCache[i];
		xsubs[i + 1] = xsubs[i];
	}
	strcpy(r.name, "** New WWIV Message Area **");
	strcpy(r.filename, "NONAME");
	r.key = 0;
	r.readsl = 10;
	r.postsl = 20;
	r.anony = 0;
	r.age = 0;
	r.maxmsgs = 50;
	r.ar = 0;
	r.type = 0;
	r.storage_type = 2;
	subboards[n] = r;
	memset( &(xsubs[n]), 0, sizeof( xtrasubsrec ) );
	++GetSession()->num_subs;
	int nNumUserRecords = GetApplication()->GetUserManager()->GetNumberOfUserRecords();

	unsigned long* pTempQScan = static_cast<unsigned long *>( BbsAllocA( syscfg.qscn_len ) );
	if ( pTempQScan )
    {
		pTempQScan_n = pTempQScan + 1;
		pTempQScan_q = pTempQScan_n + (GetSession()->GetMaxNumberFileAreas() + 31) / 32;
		pTempQScan_p = pTempQScan_q + (GetSession()->GetMaxNumberMessageAreas() + 31) / 32;

		m1 = 1L << (n % 32);
		m2 = 0xffffffff << ((n % 32) + 1);
		m3 = 0xffffffff >> (32 - (n % 32));

		for (i = 1; i <= nNumUserRecords; i++)
        {
			read_qscn(i, pTempQScan, true);

			if ( ( *pTempQScan != 999 ) && ( *pTempQScan >= static_cast<unsigned long>( n ) ) )
            {
				(*pTempQScan)++;
            }

			for (i1 = GetSession()->num_subs - 1; i1 > n; i1--)
            {
				pTempQScan_p[i1] = pTempQScan_p[i1 - 1];
            }
			pTempQScan_p[n] = 0;

			for ( i2 = GetSession()->num_subs / 32; i2 > n / 32; i2-- )
            {
				pTempQScan_q[i2] = ( pTempQScan_q[i2] << 1 ) | ( pTempQScan_q[i2 - 1] >> 31 );
			}
			pTempQScan_q[i2] = m1 | ( m2 & ( pTempQScan_q[i2] << 1 ) ) | ( m3 & pTempQScan_q[i2] );

			write_qscn( i, pTempQScan, true );
		}
		close_qscn();
		BbsFreeMemory( pTempQScan );
	}
	save_subs();

	if ( GetSession()->GetCurrentReadMessageArea() >= n )
    {
		GetSession()->SetCurrentReadMessageArea( GetSession()->GetCurrentReadMessageArea() + 1 );
    }
}


void delete_sub(int n)
{
	int i, i1, i2, nNumUserRecords;
	unsigned long *pTempQScan, *pTempQScan_n, *pTempQScan_q, *pTempQScan_p, m2, m3;
	SUBCONF_TYPE nconv = static_cast<SUBCONF_TYPE>( n );

	if ( n < 0 || n >= GetSession()->num_subs )
    {
		return;
    }

	update_conf(CONF_SUBS, &nconv, NULL, CONF_UPDATE_DELETE);

	n = static_cast<int>( nconv );

	while (xsubs[n].num_nets)
    {
		sub_xtr_del(n, 0, 1);
    }
	if ( xsubs[n].nets && ( xsubs[n].flags & XTRA_MALLOCED ) )
    {
		BbsFreeMemory(xsubs[n].nets);
    }

	for (i = n; i < GetSession()->num_subs; i++)
    {
		subboards[i] = subboards[i + 1];
		GetSession()->m_SubDateCache[i] = GetSession()->m_SubDateCache[i + 1];
		xsubs[i] = xsubs[i + 1];
	}
	--GetSession()->num_subs;
	nNumUserRecords = GetApplication()->GetUserManager()->GetNumberOfUserRecords();

	pTempQScan = static_cast<unsigned long *>( BbsAllocA( syscfg.qscn_len + 4 ) );
	if (pTempQScan)
    {
		pTempQScan_n = pTempQScan + 1;
		pTempQScan_q = pTempQScan_n + (GetSession()->GetMaxNumberFileAreas() + 31) / 32;
		pTempQScan_p = pTempQScan_q + (GetSession()->GetMaxNumberMessageAreas() + 31) / 32;

		m2 = 0xffffffff << (n % 32);
		m3 = 0xffffffff >> (32 - (n % 32));

		for (i = 1; i <= nNumUserRecords; i++)
        {
			read_qscn(i, pTempQScan, true);

			if (*pTempQScan != 999)
            {
				if (*pTempQScan == static_cast<unsigned long>( n ) )
                {
					*pTempQScan = 999;
                }
				else if (*pTempQScan > static_cast<unsigned long>( n ) )
                {
					(*pTempQScan)--;
                }
			}
			for (i1 = n; i1 < GetSession()->num_subs; i1++)
            {
				pTempQScan_p[i1] = pTempQScan_p[i1 + 1];
            }

			pTempQScan_q[n / 32] = (pTempQScan_q[n / 32] & m3) | ((pTempQScan_q[n / 32] >> 1) & m2) | (pTempQScan_q[(n / 32) + 1] << 31);

			for (i2 = (n / 32) + 1; i2 <= (GetSession()->num_subs / 32); i2++)
            {
				pTempQScan_q[i2] = (pTempQScan_q[i2] >> 1) | (pTempQScan_q[i2 + 1] << 31);
			}

			write_qscn( i, pTempQScan, true );
		}
		close_qscn();
		BbsFreeMemory( pTempQScan );
	}
	save_subs();

	if ( GetSession()->GetCurrentReadMessageArea() == n )
    {
		GetSession()->SetCurrentReadMessageArea( -1 );
    }
	else if ( GetSession()->GetCurrentReadMessageArea() > n )
    {
		GetSession()->SetCurrentReadMessageArea( GetSession()->GetCurrentReadMessageArea() - 1 );
    }
}


void boardedit()
{
	int i, i1, i2;
    bool confchg = false;
	char s[81];
	SUBCONF_TYPE iconv;

	if (!ValidateSysopPassword())
    {
		return;
    }
	showsubs();
	bool done = false;
	GetApplication()->GetStatusManager()->Read();
	do
    {
		nl();
		GetSession()->bout << "|#7(Q=Quit) (D)elete, (I)nsert, (M)odify, (S)wapSubs : ";
		char ch = onek("QSDIM?");
		switch ( ch )
        {
		case '?':
			showsubs();
			break;
		case 'Q':
			done = true;
			break;
		case 'M':
			nl();
			GetSession()->bout << "|#2Sub number? ";
			input(s, 4);
			i = atoi(s);
			if ( s[0] != 0 && i >= 0 && i < GetSession()->num_subs )
            {
				modify_sub(i);
            }
			break;
		case 'S':
			if ( GetSession()->num_subs < GetSession()->GetMaxNumberMessageAreas() )
            {
				nl();
				GetSession()->bout << "|#2Take sub number? ";
				input( s, 4 );
				i1 = atoi( s );
				if ( !s[0] || i1 < 0 || i1 >= GetSession()->num_subs )
                {
					break;
                }
				nl();
				GetSession()->bout << "|#2And move before sub number? ";
				input( s, 4 );
				i2 = atoi( s );
				if ( !s[0] || i2 < 0 || i2 % 32 == 0 || i2 > GetSession()->num_subs || i1 == i2 || i1 + 1 == i2 )
                {
					break;
                }
				nl();
				if ( i2 < i1 )
                {
					i1++;
                }
				write_qscn( GetSession()->usernum, qsc, true );
				GetSession()->bout << "|#1Moving sub now...Please wait...";
				insert_sub( i2 );
				swap_subs( i1, i2 );
				delete_sub( i1 );
				confchg = true;
				showsubs();
			}
            else
            {
				GetSession()->bout << "\r\nYou must increase the number of subs in INIT.EXE first.\r\n";
			}
			break;
		case 'I':
			if ( GetSession()->num_subs < GetSession()->GetMaxNumberMessageAreas() )
            {
				nl();
				GetSession()->bout << "|#2Insert before which sub ('$' for end) : ";
				input(s, 4);
				if (s[0] == '$')
                {
					i = GetSession()->num_subs;
                }
				else
                {
					i = atoi(s);
                }
				if ( s[0] != 0 && i >= 0 && i <= GetSession()->num_subs )
                {
					insert_sub(i);
					modify_sub(i);
					confchg = true;
					if (subconfnum > 1)
                    {
						nl();
						list_confs(CONF_SUBS, 0);
						i2 = select_conf("Put in which conference? ", CONF_SUBS, 0);
						if (i2 >= 0)
                        {
							if (in_conference(i, &subconfs[i2]) < 0)
                            {
								iconv = (SUBCONF_TYPE) i;
								addsubconf(CONF_SUBS, &subconfs[i2], &iconv);
								i = static_cast<int>( iconv );
							}
                        }
					}
                    else
                    {
						if (in_conference(i, &subconfs[0]) < 0)
                        {
							iconv = static_cast<SUBCONF_TYPE>( i );
							addsubconf(CONF_SUBS, &subconfs[0], &iconv);
							i = static_cast<int>( iconv );
						}
					}
				}
			}
			break;
		case 'D':
			nl();
			GetSession()->bout << "|#2Delete which sub? ";
			input(s, 4);
			i = atoi(s);
			if ( s[0] != 0 && i >= 0 && i < GetSession()->num_subs )
            {
				nl();
                GetSession()->bout << "|#5Delete " << subboards[i].name << "? ";
				if (yesno())
                {
					strcpy(s, subboards[i].filename);
					delete_sub(i);
					confchg = true;
					nl();
					GetSession()->bout << "|#5Delete data files (including messages) for sub also? ";
					if (yesno())
                    {
                        char szTempFileName[ MAX_PATH ];
						sprintf(szTempFileName, "%s%s.sub", syscfg.datadir, s);
						WFile::Remove(szTempFileName);
						sprintf(szTempFileName, "%s%s.dat", syscfg.msgsdir, s);
						WFile::Remove(szTempFileName);
					}
				}
			}
			break;
    }
  } while ( !done && !hangup );
  save_subs();
  if ( !GetApplication()->GetLocalIO()->GetWfcStatus() )
  {
	  changedsl();
  }
  GetSession()->subchg = 1;
  g_szMessageGatFileName[0] = '\0';
  if (confchg)
  {
	  save_confs( CONF_SUBS, -1, NULL );
  }
}
