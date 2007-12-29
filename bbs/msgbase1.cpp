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


void send_net_post(postrec * pPostRecord, const char *extra, int nSubNumber)
{
	long lMessageLength;
	char* b = readfile(&(pPostRecord->msg), extra, &lMessageLength);
	if ( b == NULL )
	{
		return;
	}

	int nNetNumber;
	int nOrigNetNumber = GetSession()->GetNetworkNumber();
	if ( pPostRecord->status & status_post_new_net )
	{
		nNetNumber = pPostRecord->title[80];
	}
	else if ( xsubs[nSubNumber].num_nets )
	{
		nNetNumber = xsubs[nSubNumber].nets[0].net_num;
	}
	else
	{
		nNetNumber = GetSession()->GetNetworkNumber();
	}

	int nn1 = nNetNumber;
	if ( pPostRecord->ownersys == 0 )
	{
		nNetNumber = -1;
	}

	net_header_rec netHeaderOrig;
	netHeaderOrig.tosys		= 0;
	netHeaderOrig.touser	= 0;
	netHeaderOrig.fromsys	= pPostRecord->ownersys;
	netHeaderOrig.fromuser	= pPostRecord->owneruser;
	netHeaderOrig.list_len	= 0;
	netHeaderOrig.daten		= pPostRecord->daten;
	netHeaderOrig.length	= lMessageLength + 1 + strlen(pPostRecord->title);
	netHeaderOrig.method	= 0;

	if ( netHeaderOrig.length > 32755 )
	{
		GetSession()->bout.WriteFormatted( "Message truncated by %lu bytes for the network.", netHeaderOrig.length - 32755L );
		netHeaderOrig.length = 32755;
		lMessageLength = netHeaderOrig.length - strlen(pPostRecord->title) - 1;
	}
	char* b1 = static_cast<char *>( BbsAllocA( netHeaderOrig.length + 100 ) );
	if ( b1 == NULL )
	{
		BbsFreeMemory( b );
		set_net_num( nOrigNetNumber );
		return;
	}
	strcpy( b1, pPostRecord->title );
	memmove( &( b1[strlen(pPostRecord->title) + 1]), b, lMessageLength );
	BbsFreeMemory( b );

	for ( int n = 0; n < xsubs[nSubNumber].num_nets; n++ )
	{
		xtrasubsnetrec* xnp = &( xsubs[nSubNumber].nets[n] );

		if ( xnp->net_num == nNetNumber && xnp->host )
		{
			continue;
		}

		set_net_num( xnp->net_num );

		net_header_rec nh = netHeaderOrig;
	    unsigned short int *pList = NULL;
		nh.minor_type = xnp->type;
		if ( !nh.fromsys )
		{
			nh.fromsys = net_sysnum;
		}

		if ( xnp->host )
		{
			nh.main_type = main_type_pre_post;
			nh.tosys = xnp->host;
		}
		else
		{
			nh.main_type = main_type_post;
			char szFileName[ MAX_PATH ];
			sprintf( szFileName, "%sn%s.net", GetSession()->GetNetworkDataDirectory(), xnp->stype );
			WFile file( szFileName );
			if ( file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
			{
				int len1 = file.GetLength();
				pList = static_cast<unsigned short int *>( BbsAllocA( len1 * 2 + 1 ) );
				if ( !pList )
				{
					continue;
				}
				if ( ( b = static_cast<char *>( BbsAllocA( len1 + 100L ) ) ) == NULL )
				{
					BbsFreeMemory( pList );
					continue;
				}
				file.Read( b, len1 );
				file.Close();
				b[len1] = '\0';
				int len2 = 0;
				while ( len2 < len1 )
				{
					while ( len2 < len1 && ( b[len2] < '0' || b[len2] > '9' ) )
					{
						++len2;
					}
					if ((b[len2] >= '0') && (b[len2] <= '9') && (len2 < len1))
					{
						int i = atoi(&(b[len2]));
						if (((GetSession()->GetNetworkNumber() != nNetNumber) || (nh.fromsys != i)) && (i != net_sysnum))
						{
							if (valid_system(i))
							{
								pList[(nh.list_len)++] = static_cast< unsigned short >( i );
							}
						}
						while ((len2 < len1) && (b[len2] >= '0') && (b[len2] <= '9'))
						{
							++len2;
						}
					}
				}
				BbsFreeMemory( b );
			}
			if ( !nh.list_len )
			{
				if ( pList )
				{
					BbsFreeMemory( pList );
				}
				continue;
			}
		}
		if (!xnp->type)
		{
			nh.main_type = main_type_new_post;
		}
		if ( nn1 == GetSession()->GetNetworkNumber() )
		{
			send_net( &nh, pList, b1, xnp->type ? NULL : xnp->stype );
		}
		else
		{
			gate_msg( &nh, b1, xnp->net_num, xnp->stype, pList, nNetNumber );
		}
		if ( pList )
		{
			BbsFreeMemory( pList );
		}
	}

	BbsFreeMemory( b1 );
	set_net_num( nOrigNetNumber );
}


void post()
{
	if ( !iscan( GetSession()->GetCurrentMessageArea() ) )
	{
		GetSession()->bout << "\r\n|12A file required is in use by another instance. Try again later.\r\n";
		return;
	}
	if ( GetSession()->GetCurrentReadMessageArea() < 0 )
	{
		GetSession()->bout << "\r\nNo subs available.\r\n\n";
		return;
	}

	if ( freek1( syscfg.msgsdir ) < 10.0 )
	{
		GetSession()->bout << "\r\nSorry, not enough disk space left.\r\n\n";
		return;
	}
    if ( GetSession()->GetCurrentUser()->IsRestrictionPost() || GetSession()->GetCurrentUser()->GetNumPostsToday() >= getslrec( GetSession()->GetEffectiveSl() ).posts )
	{
		GetSession()->bout << "\r\nToo many messages posted today.\r\n\n";
		return;
	}
	if ( GetSession()->GetEffectiveSl() < subboards[GetSession()->GetCurrentReadMessageArea()].postsl )
	{
		GetSession()->bout << "\r\nYou can't post here.\r\n\n";
		return;
	}

	messagerec m;
	m.storage_type = static_cast<unsigned char>( subboards[GetSession()->GetCurrentReadMessageArea()].storage_type );
	int a = subboards[ GetSession()->GetCurrentReadMessageArea() ].anony & 0x0f;
	if ( a == 0 && getslrec( GetSession()->GetEffectiveSl() ).ability & ability_post_anony )
	{
		a = anony_enable_anony;
	}
	if ( a == anony_enable_anony && GetSession()->GetCurrentUser()->IsRestrictionAnonymous() )
	{
		a = 0;
	}
	if ( xsubs[ GetSession()->GetCurrentReadMessageArea() ].num_nets )
	{
		a &= (anony_real_name);
		if ( GetSession()->GetCurrentUser()->IsRestrictionNet() )
		{
			GetSession()->bout << "\r\nYou can't post on networked sub-boards.\r\n\n";
			return;
		}
		if ( net_sysnum )
		{
			GetSession()->bout << "\r\nThis post will go out on ";
			for ( int i = 0; i < xsubs[ GetSession()->GetCurrentReadMessageArea() ].num_nets; i++ )
			{
				if ( i )
				{
					GetSession()->bout << ", ";
				}
				GetSession()->bout << net_networks[xsubs[GetSession()->GetCurrentReadMessageArea()].nets[i].net_num].name;
			}
			GetSession()->bout << ".\r\n\n";
		}
	}
	time_t lStartTime = time( NULL );

	write_inst( INST_LOC_POST, GetSession()->GetCurrentReadMessageArea(), INST_FLAGS_NONE );

	postrec p;
	inmsg( &m, p.title, &a, true, (subboards[GetSession()->GetCurrentReadMessageArea()].filename), INMSG_FSED,
           subboards[GetSession()->GetCurrentReadMessageArea()].name, (subboards[GetSession()->GetCurrentReadMessageArea()].anony & anony_no_tag) ? MSGED_FLAG_NO_TAGLINE : MSGED_FLAG_NONE );
	if (m.stored_as != 0xffffffff)
	{
		p.anony		= static_cast< unsigned char >( a );
		p.msg		= m;
		p.ownersys	= 0;
		p.owneruser = static_cast<unsigned short>( GetSession()->usernum );
		WStatus* pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
		p.qscan = pStatus->IncrementQScanPointer();
		GetApplication()->GetStatusManager()->CommitTransaction( pStatus );
		p.daten = static_cast<unsigned long>(time(NULL));
        if ( GetSession()->GetCurrentUser()->IsRestrictionValidate() )
		{
			p.status = status_unvalidated;
		}
		else
		{
			p.status = 0;
		}

		open_sub( true );

		if ( ( xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets ) &&
			( subboards[GetSession()->GetCurrentReadMessageArea()].anony & anony_val_net ) && ( !lcs() || irt[0]) )
		{
			p.status |= status_pending_net;
			int dm = 1;
			for ( int i = GetSession()->GetNumMessagesInCurrentMessageArea(); (i >= 1) && (i > (GetSession()->GetNumMessagesInCurrentMessageArea() - 28)); i-- )
			{
				if (get_post(i)->status & status_pending_net)
				{
					dm = 0;
					break;
				}
			}
			if (dm)
			{
				ssm(1, 0, "Unvalidated net posts on %s.", subboards[GetSession()->GetCurrentReadMessageArea()].name);
			}
		}
		if ( GetSession()->GetNumMessagesInCurrentMessageArea() >= subboards[GetSession()->GetCurrentReadMessageArea()].maxmsgs )
		{
			int i = 1;
			int dm = 0;
			while ( i <= GetSession()->GetNumMessagesInCurrentMessageArea() )
			{
				postrec* pp = get_post(i);
				if ( !pp )
				{
					break;
				}
				else if ( ( ( pp->status & status_no_delete ) == 0 ) ||
					        ( pp->msg.storage_type != subboards[GetSession()->GetCurrentReadMessageArea()].storage_type ) )
				{
					dm = i;
					break;
				}
				++i;
			}
			if (dm == 0)
			{
				dm = 1;
			}
			delete_message(dm);
		}
		add_post(&p);

        GetSession()->GetCurrentUser()->SetNumMessagesPosted( GetSession()->GetCurrentUser()->GetNumMessagesPosted() + 1 );
        GetSession()->GetCurrentUser()->SetNumPostsToday( GetSession()->GetCurrentUser()->GetNumPostsToday() + 1 );
		pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
        pStatus->IncrementNumMessagesPostedToday();
        pStatus->IncrementNumLocalPosts();

		if ( GetApplication()->HasConfigFlag( OP_FLAGS_POSTTIME_COMPENSATE ) )
		{
			time_t lEndTime = time(NULL);
			if (lStartTime > lEndTime)
			{
				lEndTime += HOURS_PER_DAY * SECONDS_PER_DAY;
			}
			lStartTime = static_cast<long>( lEndTime - lStartTime );
			if ((lStartTime / MINUTES_PER_HOUR_FLOAT ) > getslrec( GetSession()->GetEffectiveSl() ).time_per_logon)
			{
				lStartTime = static_cast<long>( static_cast<float>( getslrec( GetSession()->GetEffectiveSl() ).time_per_logon * MINUTES_PER_HOUR_FLOAT ) );
			}
			GetSession()->GetCurrentUser()->SetExtraTime( GetSession()->GetCurrentUser()->GetExtraTime() + static_cast<float>( lStartTime ) );
		}
		GetApplication()->GetStatusManager()->CommitTransaction( pStatus );
		close_sub();

		GetApplication()->UpdateTopScreen();
		sysoplogf( "+ \"%s\" posted on %s", p.title, subboards[GetSession()->GetCurrentReadMessageArea()].name );
		GetSession()->bout << "Posted on " << subboards[GetSession()->GetCurrentReadMessageArea()].name << wwiv::endl;
		if (xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets)
		{
            GetSession()->GetCurrentUser()->SetNumNetPosts( GetSession()->GetCurrentUser()->GetNumNetPosts() + 1 );
			if (!(p.status & status_pending_net))
			{
				send_net_post(&p, subboards[GetSession()->GetCurrentReadMessageArea()].filename, GetSession()->GetCurrentReadMessageArea());
			}
		}
	}
}


void grab_user_name( messagerec * pMessageRecord, const char *pszFileName )
{
	long lMessageLen;
	char* ss = readfile( pMessageRecord, pszFileName, &lMessageLen );
	if ( ss )
	{
		char* ss1 = strchr(ss, '\r');
		if ( ss1 )
		{
			*ss1 = '\0';
			char* ss2 = ss;
			if ( ss[0] == '`' && ss[1] == '`' )
			{
				for ( ss1 = ss + 2; *ss1; ss1++ )
				{
					if ( ss1[0] == '`' && ss1[1] == '`' )
					{
						ss2 = ss1 + 2;
					}
				}
				while (*ss2 == ' ')
				{
					++ss2;
				}
			}
			strcpy(net_email_name, ss2);
		}
		else
		{
			net_email_name[0] = '\0';
		}
		BbsFreeMemory(ss);
	}
	else
	{
		net_email_name[0] = '\0';
	}
}


void qscan(int nBeginSubNumber, int *pnNextSubNumber)
{
	int nSubNumber = usub[nBeginSubNumber].subnum;
	g_flags &= ~g_flag_made_find_str;

	if ( hangup || nSubNumber < 0 )
	{
		return;
	}

	GetSession()->bout.NewLine();

	unsigned long lQuickScanPointer = qsc_p[nSubNumber];

	if (!GetSession()->m_SubDateCache[nSubNumber])
	{
		iscan1(nSubNumber, true);
	}

	unsigned long lSubDate = GetSession()->m_SubDateCache[nSubNumber];
	if ( !lSubDate || lSubDate > lQuickScanPointer )
	{
		int nNextSubNumber = *pnNextSubNumber;
		int nOldSubNumber = GetSession()->GetCurrentMessageArea();
		GetSession()->SetCurrentMessageArea( nBeginSubNumber );

		if ( !iscan( GetSession()->GetCurrentMessageArea() ) )
		{
			GetSession()->bout << "\r\n\003""6A file required is in use by another instance. Try again later.\r\n";
			return;
		}
		lQuickScanPointer = qsc_p[nSubNumber];

		GetSession()->bout.WriteFormatted( "\r\n\n|#1< Q-scan %s %s - %lu msgs >\r\n", subboards[GetSession()->GetCurrentReadMessageArea()].name,
             usub[GetSession()->GetCurrentMessageArea()].keys, GetSession()->GetNumMessagesInCurrentMessageArea() );

        int i;
		for (i = GetSession()->GetNumMessagesInCurrentMessageArea(); (i > 1) && (get_post(i - 1)->qscan > lQuickScanPointer); i--)
			;

		if ( GetSession()->GetNumMessagesInCurrentMessageArea() > 0 && i <= GetSession()->GetNumMessagesInCurrentMessageArea() && get_post(i)->qscan > qsc_p[GetSession()->GetCurrentReadMessageArea()] )
		{
			scan( i, SCAN_OPTION_READ_MESSAGE, &nNextSubNumber, false );
		}
		else
		{
			WStatus *pStatus = GetApplication()->GetStatusManager()->GetStatus();
            qsc_p[GetSession()->GetCurrentReadMessageArea()] = pStatus->GetQScanPointer() - 1;
            delete pStatus;
		}

		GetSession()->SetCurrentMessageArea( nOldSubNumber );
		*pnNextSubNumber = nNextSubNumber;
		GetSession()->bout.WriteFormatted( "|#1< %s Q-Scan Done >", subboards[GetSession()->GetCurrentReadMessageArea()].name );
		GetSession()->bout.ClearEOL();
		GetSession()->bout.NewLine();
		lines_listed = 0;
		GetSession()->bout.ClearEOL();
		if ( okansi() && !newline )
		{
			GetSession()->bout << "\r\x1b[4A";
		}
	}
	else
	{
		GetSession()->bout.WriteFormatted("|#1< Nothing new on %s %s >", subboards[nSubNumber].name, usub[nBeginSubNumber].keys);
		GetSession()->bout.ClearEOL();
		GetSession()->bout.NewLine();
		lines_listed = 0;
		GetSession()->bout.ClearEOL();
		if ( okansi() && !newline )
		{
			GetSession()->bout << "\r\x1b[3A";
		}
	}
	GetSession()->bout.NewLine();
}


void nscan( int nStartingSubNum )
{
	int nNextSubNumber = 1;

	GetSession()->bout << "\r\n|#3-=< Q-Scan All >=-\r\n";
	for ( int i = nStartingSubNum; usub[i].subnum != -1 && i < GetSession()->num_subs && nNextSubNumber && !hangup; i++ )
	{
		if (qsc_q[usub[i].subnum / 32] & (1L << (usub[i].subnum % 32)))
		{
			qscan(i, &nNextSubNumber);
		}
		bool abort = false, next = false;
		checka( &abort, &next );
		if ( abort )
		{
			nNextSubNumber = 0;
		}
	}
	GetSession()->bout.NewLine();
	GetSession()->bout.ClearEOL();
	GetSession()->bout << "|#3-=< Global Q-Scan Done >=-\r\n\n";
	if ( nNextSubNumber && GetSession()->GetCurrentUser()->IsNewScanFiles() &&
		 ( syscfg.sysconfig & sysconfig_no_xfer ) == 0 &&
         ( !( g_flags & g_flag_scanned_files ) ) )
	{
		lines_listed = 0;
		GetSession()->tagging = 1;
		tmp_disable_conf( true );
		nscanall();
		tmp_disable_conf( false );
		GetSession()->tagging = 0;
	}
}



void ScanMessageTitles()
{
	if ( !iscan( GetSession()->GetCurrentMessageArea() ) )
	{
		GetSession()->bout << "\r\n|#7A file required is in use by another instance. Try again later.\r\n";
		return;
	}
	GetSession()->bout.NewLine();
	if ( GetSession()->GetCurrentReadMessageArea() < 0 )
	{
		GetSession()->bout << "No subs available.\r\n";
		return;
	}
	GetSession()->bout.WriteFormatted("|#2%d |#9messages in area |#2%s\r\n", GetSession()->GetNumMessagesInCurrentMessageArea(), subboards[GetSession()->GetCurrentReadMessageArea()].name);
	if ( GetSession()->GetNumMessagesInCurrentMessageArea() == 0 )
	{
		return;
	}
    GetSession()->bout.WriteFormatted( "|#9Start listing at (|#21|#9-|#2%d|#9): ", GetSession()->GetNumMessagesInCurrentMessageArea() );
    std::string messageNumber;
	input( messageNumber, 5, true );
    int nMessageNumber = atoi( messageNumber.c_str() );
	if (nMessageNumber < 1)
	{
		nMessageNumber = 0;
	}
	else if (nMessageNumber > GetSession()->GetNumMessagesInCurrentMessageArea())
	{
		nMessageNumber = GetSession()->GetNumMessagesInCurrentMessageArea();
	}
	else
	{
		nMessageNumber--;
	}
	int nNextSubNumber = 0;
	// 'S' means start reading at the 1st message.
	if ( messageNumber == "S" )
	{
		scan( 0, SCAN_OPTION_READ_PROMPT, &nNextSubNumber, true );
	}
	else if ( nMessageNumber >= 0 )
	{
		scan( nMessageNumber, SCAN_OPTION_LIST_TITLES, &nNextSubNumber, true );
	}
}


void delmail( WFile *pFile, int loc )
{
	mailrec m, m1;
	WUser user;

	pFile->Seek( static_cast<long>( loc * sizeof( mailrec ) ), WFile::seekBegin );
	pFile->Read( &m, sizeof( mailrec ) );

	if ( m.touser == 0 && m.tosys == 0 )
	{
		return;
	}

	bool rm = true;
	if ( m.status & status_multimail )
	{
		int t = pFile->GetLength() / sizeof( mailrec );
		int otf = false;
		for (int i = 0; i < t; i++)
		{
			if (i != loc)
			{
				pFile->Seek( static_cast<long>( i * sizeof( mailrec ) ), WFile::seekBegin );
				pFile->Read( &m1, sizeof( mailrec ) );
				if ( ( m.msg.stored_as == m1.msg.stored_as ) && ( m.msg.storage_type == m1.msg.storage_type ) && ( m1.daten != 0xffffffff ) )
				{
					otf = true;
				}
			}
		}
		if ( otf )
		{
			rm = false;
		}
	}
	if ( rm )
	{
		remove_link( &m.msg, "email" );
	}

	if ( m.tosys == 0 )
	{
        GetApplication()->GetUserManager()->ReadUser( &user, m.touser );
        if ( user.GetNumMailWaiting() )
		{
            user.SetNumMailWaiting( user.GetNumMailWaiting() - 1 );
            GetApplication()->GetUserManager()->WriteUser( &user, m.touser );
		}
		if ( m.touser == 1 )
		{
			--fwaiting;
		}
	}
	pFile->Seek( static_cast<long>( loc * sizeof( mailrec ) ), WFile::seekBegin );
	m.touser = 0;
	m.tosys = 0;
	m.daten = 0xffffffff;
	m.msg.storage_type = 0;
	m.msg.stored_as = 0xffffffff;
	pFile->Write( &m, sizeof( mailrec ) );
	mailcheck = true;
}


void remove_post()
{
    if ( !iscan( GetSession()->GetCurrentMessageArea() ) )
	{
		GetSession()->bout << "\r\n|12A file required is in use by another instance. Try again later.\r\n\n";
		return;
	}
	if (GetSession()->GetCurrentReadMessageArea() < 0)
	{
		GetSession()->bout << "\r\nNo subs available.\r\n\n";
		return;
	}
	bool any = false, abort = false;
	GetSession()->bout.WriteFormatted("\r\n\nPosts by you on %s\r\n\n", subboards[GetSession()->GetCurrentReadMessageArea()].name);
	for ( int j = 1; j <= GetSession()->GetNumMessagesInCurrentMessageArea() && !abort; j++ )
	{
		if ( get_post( j )->ownersys == 0 && get_post( j )->owneruser == GetSession()->usernum )
		{
			any = true;
            std::string buffer;
            wwiv::stringUtils::FormatString( buffer, "%u: %60.60s", j, get_post( j )->title );
            pla( buffer.c_str(), &abort );
		}
	}
	if ( !any )
	{
		GetSession()->bout << "None.\r\n";
		if (!cs())
		{
			return;
		}
	}
	GetSession()->bout << "\r\n|#2Remove which? ";
    std::string postNumberToRemove;
	input( postNumberToRemove, 5 );
	int nPostNumber = atoi( postNumberToRemove.c_str() );
	open_sub( true );
	if ( nPostNumber > 0 && nPostNumber <= GetSession()->GetNumMessagesInCurrentMessageArea() )
	{
		if ( ( ( get_post( nPostNumber )->ownersys == 0 ) && ( get_post( nPostNumber )->owneruser == GetSession()->usernum ) ) || lcs() )
		{
			if ( ( get_post( nPostNumber )->owneruser == GetSession()->usernum ) && ( get_post( nPostNumber )->ownersys == 0 ) )
			{
                WUser tu;
                GetApplication()->GetUserManager()->ReadUser( &tu, get_post( nPostNumber )->owneruser  );
                if ( !tu.IsUserDeleted() )
				{
                    if ( date_to_daten( tu.GetFirstOn() ) < ( signed ) get_post( nPostNumber )->daten )
					{
                        if ( tu.GetNumMessagesPosted() )
						{
							tu.SetNumMessagesPosted( tu.GetNumMessagesPosted() -1 );
                            GetApplication()->GetUserManager()->WriteUser( &tu, get_post( nPostNumber )->owneruser  );
						}
					}
				}
			}
			sysoplogf("- \"%s\" removed from %s", get_post( nPostNumber )->title, subboards[GetSession()->GetCurrentReadMessageArea()].name);
			delete_message( nPostNumber );
			GetSession()->bout << "\r\nMessage removed.\r\n\n";
		}
	}
	close_sub();
}



bool external_edit( const char *pszEditFileName, const char *pszNewDirectory, int nEditorNumber, int numlines, const char *pszDestination, const char *pszTitle, int flags )
{
	char szFileName[ MAX_PATH ], sx1[21], sx2[21], sx3[21], szCurrentDirectory[ MAX_PATH ];
	time_t tFileTime = 0;

	if ( nEditorNumber >= GetSession()->GetNumberOfEditors() || !okansi() )
	{
		GetSession()->bout << "\r\nYou can't use that full screen editor.\r\n\n";
		return false;
	}
	std::string editorCommand = ( incom ) ? editors[ nEditorNumber ].filename : editors[ nEditorNumber ].filenamecon;

	if ( editorCommand.empty() )
	{
		GetSession()->bout << "\r\nYou can't use that full screen editor.\r\n\n";
		return false;
	}
	WWIV_make_abs_cmd( editorCommand );

	WWIV_GetDir( szCurrentDirectory, false );
	WWIV_ChangeDirTo( pszNewDirectory );

    WFile::SetFilePermissions( EDITOR_INF, WFile::permReadWrite );
	WFile::Remove( EDITOR_INF );
	WFile::SetFilePermissions( RESULT_ED, WFile::permReadWrite );
	WFile::Remove( RESULT_ED );

    std::string strippedFileName;
	strippedFileName = stripfn( pszEditFileName );
	if ( pszNewDirectory[0] )
	{
		WWIV_ChangeDirTo( pszNewDirectory) ;
		WWIV_GetDir( szFileName, true );
		WWIV_ChangeDirTo( pszNewDirectory );
	}
	else
	{
		szFileName[0] = '\0';
	}
    strcat( szFileName, strippedFileName.c_str() );
	WFile fileTempForTime( szFileName );
	bool bIsFileThere = fileTempForTime.Exists();
	if ( bIsFileThere )
	{
		tFileTime = fileTempForTime.GetFileTime();
	}
    sprintf( sx1, "%d", GetSession()->GetCurrentUser()->GetScreenChars() );
	int newtl = ( GetSession()->screenlinest > defscreenbottom - GetSession()->localIO()->GetTopLine() ) ? 0 : GetSession()->localIO()->GetTopLine();

	if ( GetSession()->using_modem )
	{
		sprintf( sx2, "%d", GetSession()->GetCurrentUser()->GetScreenLines() );
	}
	else
	{
        sprintf( sx2, "%ld", defscreenbottom + 1 - newtl );
	}
	sprintf( sx3, "%d", numlines );
	const std::string cmdLine = stuff_in( editorCommand, szFileName, sx1, sx2, sx3, "" );
    
    WTextFile fileEditorInf( EDITOR_INF, "wt" );

    if ( fileEditorInf.IsOpen() )
	{
		if (irt_name[0])
		{
			flags |= MSGED_FLAG_HAS_REPLY_NAME;
		}
		if (irt[0])
		{
			flags |= MSGED_FLAG_HAS_REPLY_TITLE;
		}
        fileEditorInf.WriteFormatted(
					"%s\n%s\n%lu\n%s\n%s\n%u\n%u\n%lu\n%u\n",
		            pszTitle,
		            pszDestination,
		            GetSession()->usernum,
		            GetSession()->GetCurrentUser()->GetName(),
		            GetSession()->GetCurrentUser()->GetRealName(),
		            GetSession()->GetCurrentUser()->GetSl(),
		            flags,
		            GetSession()->localIO()->GetTopLine(),
		            GetSession()->GetCurrentUser()->GetLanguage() );
        fileEditorInf.Close();
	}
	if ( flags & MSGED_FLAG_NO_TAGLINE )
	{
		// disable tag lines by creating a DISABLE.TAG file
        WTextFile fileDisableTag( DISABLE_TAG, "w" );
	}
	else
	{
		WFile::Remove( DISABLE_TAG );
	}
	if (!irt[0])
	{
		WFile::Remove( QUOTES_TXT );
		WFile::Remove( QUOTES_IND );
	}
	ExecuteExternalProgram( cmdLine, GetApplication()->GetSpawnOptions( SPWANOPT_FSED ) );
	lines_listed = 0;
	WWIV_ChangeDirTo( pszNewDirectory );
	WFile::Remove( EDITOR_INF );
	WFile::Remove( DISABLE_TAG );
	bool bModifiedOrExists = false;
	if ( !bIsFileThere )
	{
		bModifiedOrExists = WFile::Exists( szFileName );
	}
	else
	{
		WFile fileTempForTime( szFileName );
		if ( fileTempForTime.Exists() )
		{
			time_t tFileTime1 = fileTempForTime.GetFileTime();
			if ( tFileTime != tFileTime1 )
			{
				bModifiedOrExists = true;
			}
		}
	}
	WWIV_ChangeDirTo( szCurrentDirectory );
	return bModifiedOrExists;
}
