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



void send_net_post(postrec * pPostRecord, const char *extra, int nSubNumber)
{
	long lMessageLength;
	char* b = readfile(&(pPostRecord->msg), extra, &lMessageLength);
	if ( b == NULL )
	{
		return;
	}

	int nNetNumber;
	int nOrigNetNumber = sess->GetNetworkNumber();
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
		nNetNumber = sess->GetNetworkNumber();
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
		bprintf( "Message truncated by %lu bytes for the network.", netHeaderOrig.length - 32755L );
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
			sprintf( szFileName, "%sn%s.net", sess->GetNetworkDataDirectory(), xnp->stype );
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
						if (((sess->GetNetworkNumber() != nNetNumber) || (nh.fromsys != i)) && (i != net_sysnum))
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
		if ( nn1 == sess->GetNetworkNumber() )
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
	if ( !iscan( sess->GetCurrentMessageArea() ) )
	{
		sess->bout << "\r\n|12A file required is in use by another instance. Try again later.\r\n";
		return;
	}
	if ( sess->GetCurrentReadMessageArea() < 0 )
	{
		sess->bout << "\r\nNo subs available.\r\n\n";
		return;
	}

	if ( freek1( syscfg.msgsdir ) < 10.0 )
	{
		sess->bout << "\r\nSorry, not enough disk space left.\r\n\n";
		return;
	}
    if ( sess->thisuser.isRestrictionPost() || sess->thisuser.GetNumPostsToday() >= getslrec( sess->GetEffectiveSl() ).posts )
	{
		sess->bout << "\r\nToo many messages posted today.\r\n\n";
		return;
	}
	if ( sess->GetEffectiveSl() < subboards[sess->GetCurrentReadMessageArea()].postsl )
	{
		sess->bout << "\r\nYou can't post here.\r\n\n";
		return;
	}

	messagerec m;
	m.storage_type = static_cast<unsigned char>( subboards[sess->GetCurrentReadMessageArea()].storage_type );
	int a = subboards[ sess->GetCurrentReadMessageArea() ].anony & 0x0f;
	if ( a == 0 && getslrec( sess->GetEffectiveSl() ).ability & ability_post_anony )
	{
		a = anony_enable_anony;
	}
	if ( a == anony_enable_anony && sess->thisuser.isRestrictionAnonymous() )
	{
		a = 0;
	}
	if ( xsubs[ sess->GetCurrentReadMessageArea() ].num_nets )
	{
		a &= (anony_real_name);
		if ( sess->thisuser.isRestrictionNet() )
		{
			sess->bout << "\r\nYou can't post on networked sub-boards.\r\n\n";
			return;
		}
		if ( net_sysnum )
		{
			sess->bout << "\r\nThis post will go out on ";
			for ( int i = 0; i < xsubs[ sess->GetCurrentReadMessageArea() ].num_nets; i++ )
			{
				if ( i )
				{
					sess->bout << ", ";
				}
				sess->bout << net_networks[xsubs[sess->GetCurrentReadMessageArea()].nets[i].net_num].name;
			}
			sess->bout << ".\r\n\n";
		}
	}
	time_t lStartTime = time( NULL );

	write_inst( INST_LOC_POST, sess->GetCurrentReadMessageArea(), INST_FLAGS_NONE );

	postrec p;
	inmsg( &m, p.title, &a, true, (subboards[sess->GetCurrentReadMessageArea()].filename), INMSG_FSED,
           subboards[sess->GetCurrentReadMessageArea()].name, (subboards[sess->GetCurrentReadMessageArea()].anony & anony_no_tag) ? MSGED_FLAG_NO_TAGLINE : MSGED_FLAG_NONE );
	if (m.stored_as != 0xffffffff)
	{
		p.anony		= static_cast< unsigned char >( a );
		p.msg		= m;
		p.ownersys	= 0;
		p.owneruser = static_cast<unsigned short>( sess->usernum );
		app->statusMgr->Lock();
		p.qscan = status.qscanptr++;
		app->statusMgr->Write();
		time((long *) (&p.daten));
        if ( sess->thisuser.isRestrictionValidate() )
		{
			p.status = status_unvalidated;
		}
		else
		{
			p.status = 0;
		}

		open_sub( true );

		if ( ( xsubs[sess->GetCurrentReadMessageArea()].num_nets ) &&
			( subboards[sess->GetCurrentReadMessageArea()].anony & anony_val_net ) && ( !lcs() || irt[0]) )
		{
			p.status |= status_pending_net;
			int dm = 1;
			for ( int i = sess->GetNumMessagesInCurrentMessageArea(); (i >= 1) && (i > (sess->GetNumMessagesInCurrentMessageArea() - 28)); i-- )
			{
				if (get_post(i)->status & status_pending_net)
				{
					dm = 0;
					break;
				}
			}
			if (dm)
			{
				ssm(1, 0, "Unvalidated net posts on %s.", subboards[sess->GetCurrentReadMessageArea()].name);
			}
		}
		if ( sess->GetNumMessagesInCurrentMessageArea() >= subboards[sess->GetCurrentReadMessageArea()].maxmsgs )
		{
			int i = 1;
			int dm = 0;
			while ( i <= sess->GetNumMessagesInCurrentMessageArea() )
			{
				postrec* pp = get_post(i);
				if ( !pp )
				{
					break;
				}
				else if ( ( ( pp->status & status_no_delete ) == 0 ) ||
					        ( pp->msg.storage_type != subboards[sess->GetCurrentReadMessageArea()].storage_type ) )
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

        sess->thisuser.SetNumMessagesPosted( sess->thisuser.GetNumMessagesPosted() + 1 );
        sess->thisuser.SetNumPostsToday( sess->thisuser.GetNumPostsToday() + 1 );
		app->statusMgr->Lock();
		++status.msgposttoday;
		++status.localposts;

		if ( app->HasConfigFlag( OP_FLAGS_POSTTIME_COMPENSATE ) )
		{
			time_t lEndTime = time(NULL);
			if (lStartTime > lEndTime)
			{
				lEndTime += HOURS_PER_DAY * SECONDS_PER_DAY;
			}
			lStartTime = static_cast<long>( lEndTime - lStartTime );
			if ((lStartTime / MINUTES_PER_HOUR_FLOAT ) > getslrec( sess->GetEffectiveSl() ).time_per_logon)
			{
				lStartTime = static_cast<long>( static_cast<float>( getslrec( sess->GetEffectiveSl() ).time_per_logon * MINUTES_PER_HOUR_FLOAT ) );
			}
			sess->thisuser.SetExtraTime( sess->thisuser.GetExtraTime() + static_cast<float>( lStartTime ) );
		}
		app->statusMgr->Write();
		close_sub();

		app->localIO->UpdateTopScreen();
		sysoplogf( "+ \"%s\" posted on %s", p.title, subboards[sess->GetCurrentReadMessageArea()].name );
		sess->bout << "Posted on " << subboards[sess->GetCurrentReadMessageArea()].name << wwiv::endl;
		if (xsubs[sess->GetCurrentReadMessageArea()].num_nets)
		{
            sess->thisuser.SetNumNetPosts( sess->thisuser.GetNumNetPosts() + 1 );
			if (!(p.status & status_pending_net))
			{
				send_net_post(&p, subboards[sess->GetCurrentReadMessageArea()].filename, sess->GetCurrentReadMessageArea());
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

	nl();

	unsigned long lQuickScanPointer = qsc_p[nSubNumber];

	if (!sess->m_SubDateCache[nSubNumber])
	{
		iscan1(nSubNumber, true);
	}

	unsigned long lSubDate = sess->m_SubDateCache[nSubNumber];
	if ( !lSubDate || lSubDate > lQuickScanPointer )
	{
		int nNextSubNumber = *pnNextSubNumber;
		int nOldSubNumber = sess->GetCurrentMessageArea();
		sess->SetCurrentMessageArea( nBeginSubNumber );

		if ( !iscan( sess->GetCurrentMessageArea() ) )
		{
			sess->bout << "\r\n\003""6A file required is in use by another instance. Try again later.\r\n";
			return;
		}
		lQuickScanPointer = qsc_p[nSubNumber];

		bprintf( "\r\n\n|#1< Q-scan %s %s - %lu msgs >\r\n", subboards[sess->GetCurrentReadMessageArea()].name,
             usub[sess->GetCurrentMessageArea()].keys, sess->GetNumMessagesInCurrentMessageArea() );

        int i;
		for (i = sess->GetNumMessagesInCurrentMessageArea(); (i > 1) && (get_post(i - 1)->qscan > lQuickScanPointer); i--)
			;

		if ( sess->GetNumMessagesInCurrentMessageArea() > 0 && i <= sess->GetNumMessagesInCurrentMessageArea() && get_post(i)->qscan > qsc_p[sess->GetCurrentReadMessageArea()] )
		{
			scan( i, SCAN_OPTION_READ_MESSAGE, &nNextSubNumber, false );
		}
		else
		{
			app->statusMgr->Read();
			qsc_p[sess->GetCurrentReadMessageArea()] = status.qscanptr - 1;
		}

		sess->SetCurrentMessageArea( nOldSubNumber );
		*pnNextSubNumber = nNextSubNumber;
		bprintf( "|#1< %s Q-Scan Done >", subboards[sess->GetCurrentReadMessageArea()].name );
		ClearEOL();
		nl();
		lines_listed = 0;
		ClearEOL();
		if ( okansi() && !newline )
		{
			sess->bout << "\r\x1b[4A";
		}
	}
	else
	{
		bprintf("|#1< Nothing new on %s %s >", subboards[nSubNumber].name, usub[nBeginSubNumber].keys);
		ClearEOL();
		nl();
		lines_listed = 0;
		ClearEOL();
		if ( okansi() && !newline )
		{
			sess->bout << "\r\x1b[3A";
		}
	}
	nl();
}


void nscan( int nStartingSubNum )
{
	int nNextSubNumber = 1;

	sess->bout << "\r\n|#3-=< Q-Scan All >=-\r\n";
	for ( int i = nStartingSubNum; usub[i].subnum != -1 && i < sess->num_subs && nNextSubNumber && !hangup; i++ )
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
	nl();
	ClearEOL();
	sess->bout << "|#3-=< Global Q-Scan Done >=-\r\n\n";
	if ( nNextSubNumber && sess->thisuser.isNewScanFiles() &&
		 ( syscfg.sysconfig & sysconfig_no_xfer ) == 0 &&
         ( !( g_flags & g_flag_scanned_files ) ) )
	{
		lines_listed = 0;
		sess->tagging = 1;
		tmp_disable_conf( true );
		nscanall();
		tmp_disable_conf( false );
		sess->tagging = 0;
	}
}



void ScanMessageTitles()
{
	if ( !iscan( sess->GetCurrentMessageArea() ) )
	{
		sess->bout << "\r\n|#7A file required is in use by another instance. Try again later.\r\n";
		return;
	}
	nl();
	if ( sess->GetCurrentReadMessageArea() < 0 )
	{
		sess->bout << "No subs available.\r\n";
		return;
	}
	bprintf("|#2%d |#9messages in area |#2%s\r\n", sess->GetNumMessagesInCurrentMessageArea(), subboards[sess->GetCurrentReadMessageArea()].name);
	if ( sess->GetNumMessagesInCurrentMessageArea() == 0 )
	{
		return;
	}
    bprintf( "|#9Start listing at (|#21|#9-|#2%d|#9): ", sess->GetNumMessagesInCurrentMessageArea() );
    std::string messageNumber;
	input( messageNumber, 5, true );
    int nMessageNumber = atoi( messageNumber.c_str() );
	if (nMessageNumber < 1)
	{
		nMessageNumber = 0;
	}
	else if (nMessageNumber > sess->GetNumMessagesInCurrentMessageArea())
	{
		nMessageNumber = sess->GetNumMessagesInCurrentMessageArea();
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
        app->userManager->ReadUser( &user, m.touser );
        if ( user.GetNumMailWaiting() )
		{
            user.SetNumMailWaiting( user.GetNumMailWaiting() - 1 );
            app->userManager->WriteUser( &user, m.touser );
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
    if ( !iscan( sess->GetCurrentMessageArea() ) )
	{
		sess->bout << "\r\n|12A file required is in use by another instance. Try again later.\r\n\n";
		return;
	}
	if (sess->GetCurrentReadMessageArea() < 0)
	{
		sess->bout << "\r\nNo subs available.\r\n\n";
		return;
	}
	bool any = false, abort = false;
	bprintf("\r\n\nPosts by you on %s\r\n\n", subboards[sess->GetCurrentReadMessageArea()].name);
	for ( int j = 1; j <= sess->GetNumMessagesInCurrentMessageArea() && !abort; j++ )
	{
		if ( get_post( j )->ownersys == 0 && get_post( j )->owneruser == sess->usernum )
		{
			any = true;
            std::string buffer;
            wwiv::stringUtils::FormatString( buffer, "%u: %60.60s", j, get_post( j )->title );
            pla( buffer.c_str(), &abort );
		}
	}
	if ( !any )
	{
		sess->bout << "None.\r\n";
		if (!cs())
		{
			return;
		}
	}
	sess->bout << "\r\n|#2Remove which? ";
    std::string postNumberToRemove;
	input( postNumberToRemove, 5 );
	int nPostNumber = atoi( postNumberToRemove.c_str() );
	open_sub( true );
	if ( nPostNumber > 0 && nPostNumber <= sess->GetNumMessagesInCurrentMessageArea() )
	{
		if ( ( ( get_post( nPostNumber )->ownersys == 0 ) && ( get_post( nPostNumber )->owneruser == sess->usernum ) ) || lcs() )
		{
			if ( ( get_post( nPostNumber )->owneruser == sess->usernum ) && ( get_post( nPostNumber )->ownersys == 0 ) )
			{
                WUser tu;
                app->userManager->ReadUser( &tu, get_post( nPostNumber )->owneruser  );
                if ( !tu.isUserDeleted() )
				{
                    if ( date_to_daten( tu.GetFirstOn() ) < ( signed ) get_post( nPostNumber )->daten )
					{
                        if ( tu.GetNumMessagesPosted() )
						{
							tu.SetNumMessagesPosted( tu.GetNumMessagesPosted() -1 );
                            app->userManager->WriteUser( &tu, get_post( nPostNumber )->owneruser  );
						}
					}
				}
			}
			sysoplogf("- \"%s\" removed from %s", get_post( nPostNumber )->title, subboards[sess->GetCurrentReadMessageArea()].name);
			delete_message( nPostNumber );
			sess->bout << "\r\nMessage removed.\r\n\n";
		}
	}
	close_sub();
}



bool external_edit( const char *pszEditFileName, const char *pszNewDirectory, int nEditorNumber, int numlines, const char *pszDestination, const char *pszTitle, int flags )
{
	char szCmdLine[ MAX_PATH ], szEditorCommand[ MAX_PATH ], szFileName[ MAX_PATH ], sx1[21], sx2[21], sx3[21], szCurrentDirectory[ MAX_PATH ];
	time_t tFileTime = 0;
	FILE *fpFile;

	if ( nEditorNumber >= sess->GetNumberOfEditors() || !okansi() )
	{
		sess->bout << "\r\nYou can't use that full screen editor.\r\n\n";
		return false;
	}
	strcpy( szEditorCommand, ( incom ) ? editors[ nEditorNumber ].filename : editors[ nEditorNumber ].filenamecon );

	if ( szEditorCommand[0] == '\0' )
	{
		sess->bout << "\r\nYou can't use that full screen editor.\r\n\n";
		return false;
	}
	WWIV_make_abs_cmd( szEditorCommand );

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
    sprintf( sx1, "%d", sess->thisuser.GetScreenChars() );
	int newtl = ( sess->screenlinest > defscreenbottom - sess->topline ) ? 0 : sess->topline;

	if ( sess->using_modem )
	{
		sprintf( sx2, "%d", sess->thisuser.GetScreenLines() );
	}
	else
	{
        sprintf( sx2, "%ld", defscreenbottom + 1 - newtl );
	}
	sprintf( sx3, "%d", numlines );
	stuff_in( szCmdLine, szEditorCommand, szFileName, sx1, sx2, sx3, "" );

	if ( ( fpFile = fsh_open( EDITOR_INF, "wt" ) ) != NULL )
	{
		if (irt_name[0])
		{
			flags |= MSGED_FLAG_HAS_REPLY_NAME;
		}
		if (irt[0])
		{
			flags |= MSGED_FLAG_HAS_REPLY_TITLE;
		}
		fprintf(	fpFile,
					"%s\n%s\n%lu\n%s\n%s\n%u\n%u\n%lu\n%u\n",
		            pszTitle,
		            pszDestination,
		            sess->usernum,
		            sess->thisuser.GetName(),
		            sess->thisuser.GetRealName(),
		            sess->thisuser.GetSl(),
		            flags,
		            sess->topline,
		            sess->thisuser.GetLanguage() );
		fsh_close(fpFile);
	}
	if ( flags & 1 )
	{
		// disable tag lines
		fpFile = fsh_open( DISABLE_TAG, "w" );
		if ( fpFile > 0 )
		{
			fsh_close( fpFile );
		}
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
	ExecuteExternalProgram(szCmdLine, app->GetSpawnOptions( SPWANOPT_FSED ) );
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
