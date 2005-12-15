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

#include "wwiv.h"

//
// local function prototypes
//
void add_list(int *pnUserNumber, int *numu, int maxu, int allowdup);
int  oneuser();


#define EMAIL_STORAGE 2


void multimail(int *pnUserNumber, int numu)
{
	int i1, cv;
	mailrec m, m1;
	char s[255], t[81], s1[81], s2[81];
	WUser user;

	if (freek1(syscfg.msgsdir) < 10.0)
	{
		nl();
		GetSession()->bout << "Sorry, not enough disk space left.\r\n\n";
		return;
	}
	nl();

	int i = 0;
	if (getslrec( GetSession()->GetEffectiveSl() ).ability & ability_email_anony)
	{
		i = anony_enable_anony;
	}
	GetSession()->bout << "|#5Show all recipients in mail? ";
	bool show_all = yesno();
	int j = 0;
	strcpy(s1, "\003""6CC: \003""1");

	m.msg.storage_type = EMAIL_STORAGE;
	strcpy(irt, "Multi-Mail");
	irt_name[0] = 0;
	WFile::Remove( QUOTES_TXT );
	inmsg( &m.msg, t, &i, true, "email", INMSG_FSED, "Multi-Mail", MSGED_FLAG_NONE );
	if ( m.msg.stored_as == 0xffffffff )
	{
		return;
	}
	strcpy( m.title, t );

	GetSession()->bout <<  "Mail sent to:\r\n";
	sysoplog( "Multi-Mail to:" );

	lineadd( &m.msg, "\003""7----", "email" );

	for ( cv = 0; cv < numu; cv++ )
	{
		if (pnUserNumber[cv] < 0)
		{
			continue;
		}
        GetApplication()->GetUserManager()->ReadUser( &user, pnUserNumber[cv] );
        if ( ( user.GetSl() == 255 && ( user.GetNumMailWaiting() > (syscfg.maxwaiting * 5) ) ) ||
			( ( user.GetSl() != 255) && ( user.GetNumMailWaiting() > syscfg.maxwaiting ) ) ||
			user.GetNumMailWaiting() > 200 )
		{
            GetSession()->bout << user.GetUserNameAndNumber( pnUserNumber[cv] ) << " mailbox full, not sent.";
			pnUserNumber[cv] = -1;
			continue;
		}
        if ( user.isUserDeleted() )
		{
			GetSession()->bout << "User deleted, not sent.\r\n";
			pnUserNumber[cv] = -1;
			continue;
		}
		strcpy(s, "  ");
        user.SetNumMailWaiting( user.GetNumMailWaiting() + 1 );
        GetApplication()->GetUserManager()->WriteUser( &user, pnUserNumber[cv] );
		if (pnUserNumber[cv] == 1)
		{
			++fwaiting;
		}
        strcat(s, user.GetUserNameAndNumber( pnUserNumber[cv] ) );
		WStatus* pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
		if (pnUserNumber[cv] == 1)
		{
            pStatus->IncrementNumFeedbackSentToday();
            GetSession()->thisuser.SetNumFeedbackSentToday( GetSession()->thisuser.GetNumFeedbackSentToday() + 1 );
            GetSession()->thisuser.SetNumFeedbackSent( GetSession()->thisuser.GetNumFeedbackSent() + 1 );
			++fsenttoday;
		}
		else
		{
            pStatus->IncrementNumEmailSentToday();
            GetSession()->thisuser.SetNumEmailSent( GetSession()->thisuser.GetNumEmailSent() + 1 );
            GetSession()->thisuser.SetNumEmailSentToday( GetSession()->thisuser.GetNumEmailSentToday() + 1 );
		}
		GetApplication()->GetStatusManager()->CommitTransaction( pStatus );
		sysoplog(s);
		GetSession()->bout << s;
		nl();
		if (show_all)
		{
            sprintf(s2, "%-22.22s  ", user.GetUserNameAndNumber( pnUserNumber[cv] ) );
			strcat(s1, s2);
			j++;
			if (j >= 3)
			{
				lineadd(&m.msg, s1, "email");
				j = 0;
				strcpy(s1, "\003""1    ");
			}
		}
	}
	if (show_all)
	{
		if (j)
		{
			lineadd(&m.msg, s1, "email");
		}
	}
	sprintf(s1, "\003""2Mail Sent to %d Addresses!", numu);
	lineadd(&m.msg, "\003""7----", "email");
	lineadd(&m.msg, s1, "email");

	m.anony = static_cast< unsigned char >( i );
	m.fromsys = 0;
	m.fromuser = static_cast<unsigned short>( GetSession()->usernum );
	m.tosys = 0;
	m.touser = 0;
	m.status = status_multimail;
	m.daten = static_cast<unsigned long>(time(NULL));

	WFile *pFileEmail = OpenEmailFile( true );
	int len = pFileEmail->GetLength() / sizeof(mailrec);
	if (len == 0)
	{
		i = 0;
	}
	else
	{
		i = len - 1;
		pFileEmail->Seek( static_cast<long>( i ) * sizeof( mailrec ), WFile::seekBegin );
		pFileEmail->Read( &m1, sizeof( mailrec ) );
		while ((i > 0) && (m1.tosys == 0) && (m1.touser == 0))
		{
			--i;
			pFileEmail->Seek( static_cast<long>( i ) * sizeof( mailrec ), WFile::seekBegin );
			i1 = pFileEmail->Read( &m1, sizeof( mailrec ) );
			if (i1 == -1)
			{
				GetSession()->bout << "|#6DIDN'T READ WRITE!\r\n";
			}
		}
		if ((m1.tosys) || (m1.touser))
		{
			++i;
		}
	}
	pFileEmail->Seek( static_cast<long>( i ) * sizeof( mailrec ), WFile::seekBegin );
	for (cv = 0; cv < numu; cv++)
	{
		if (pnUserNumber[cv] > 0)
		{
			m.touser = static_cast<unsigned short>( pnUserNumber[cv] );
			pFileEmail->Write( &m, sizeof( mailrec ) );
		}
	}
	pFileEmail->Close();
	delete pFileEmail;
}


char *mml_s;
int mml_started;


int oneuser()
{
	char s[81], *ss;
	int nUserNumber, nSystemNumber, i;
	WUser user;

	if (mml_s)
	{
		if (mml_started)
		{
			ss = strtok(NULL, "\r\n");
		}
		else
		{
			ss = strtok(mml_s, "\r\n");
		}
		mml_started = 1;
		if (ss == NULL)
		{
			BbsFreeMemory(mml_s);
			mml_s = NULL;
			return -1;
		}
		strcpy(s, ss);
		for (i = 0; s[i] != 0; i++)
		{
			s[i] = upcase(s[i]);
		}
	}
	else
	{
		GetSession()->bout << "|#2>";
		input(s, 40);
	}
	nUserNumber = finduser1(s);
	if (nUserNumber == 65535)
	{
		return -1;
	}
	if (s[0] == 0)
	{
		return -1;
	}
	if (nUserNumber <= 0)
	{
		nl();
		GetSession()->bout << "Unknown user.\r\n\n";
		return 0;
	}
	nSystemNumber = 0;
	if (ForwardMessage(&nUserNumber, &nSystemNumber))
	{
		nl();
		GetSession()->bout << "Forwarded.\r\n\n";
		if (nSystemNumber)
		{
			GetSession()->bout << "Forwarded to another system.\r\n";
			GetSession()->bout << "Can't send multi-mail to another system.\r\n\n";
			return 0;
		}
	}
	if (nUserNumber == 0)
	{
		nl();
		GetSession()->bout << "Unknown user.\r\n\n";
		return 0;
	}
    GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
    if ((( user.GetSl() == 255) && ( user.GetNumMailWaiting() > (syscfg.maxwaiting * 5))) ||
        (( user.GetSl() != 255) && ( user.GetNumMailWaiting() > syscfg.maxwaiting)) ||
        ( user.GetNumMailWaiting() > 200))
	{
		nl();
		GetSession()->bout << "Mailbox full.\r\n\n";
		return 0;
	}
    if ( user.isUserDeleted() )
	{
		nl();
		GetSession()->bout << "Deleted user.\r\n\n";
		return 0;
	}
	GetSession()->bout << "     -> " << user.GetUserNameAndNumber( nUserNumber ) << wwiv::endl;
	return nUserNumber;
}


void add_list(int *pnUserNumber, int *numu, int maxu, int allowdup)
{
	bool done = false;
	int mml = (mml_s != NULL);
	mml_started = 0;
	while ( !done && !hangup && ( *numu < maxu ) )
	{
		int i = oneuser();
		if (mml && (!mml_s))
		{
			done = true;
		}
		if (i == -1)
		{
			done = true;
		}
		else if (i)
		{
			if (!allowdup)
			{
				for (int i1 = 0; i1 < *numu; i1++)
				{
					if (pnUserNumber[i1] == i)
					{
						nl();
						GetSession()->bout << "Already in list, not added.\r\n\n";
						i = 0;
					}
					if (i)
					{
						pnUserNumber[(*numu)++] = i;
					}
				}
			}
		}
	}
	if (*numu == maxu)
	{
		nl();
		GetSession()->bout << "List full.\r\n\n";
	}
}


#define MAX_LIST 40


void slash_e()
{
	int nUserNumber[MAX_LIST], numu, i, i1;
	char s[81], ch, *sss;
	bool bFound = false;
	WFindFile fnd;

	mml_s = NULL;
	mml_started = 0;
	if (freek1(syscfg.msgsdir) < 10.0)
	{
		nl();
		GetSession()->bout << "Sorry, not enough disk space left.\r\n\n";
		return;
	}
    if (((fsenttoday >= 5) || (GetSession()->thisuser.GetNumFeedbackSentToday() >= 10) ||
		(GetSession()->thisuser.GetNumEmailSentToday() >= getslrec( GetSession()->GetEffectiveSl() ).emails)) && (!cs()))
	{
		GetSession()->bout << "Too much mail sent today.\r\n\n";
		return;
	}
	if ( GetSession()->thisuser.isRestrictionEmail() )
	{
		GetSession()->bout << "You can't send mail.\r\n";
		return;
	}
	bool done = false;
	numu = 0;
	do
	{
		nl( 2 );
		GetSession()->bout << "|#2Multi-Mail: A,M,D,L,E,Q,? : ";
		ch = onek("QAMDEL?");
		switch (ch)
		{
		case '?':
			printfile(MMAIL_NOEXT);
			break;
		case 'Q':
			done = true;
			break;
		case 'A':
			nl();
			GetSession()->bout << "Enter names/numbers for users, one per line, max 20.\r\n\n";
			mml_s = NULL;
			add_list(nUserNumber, &numu, MAX_LIST, so());
			break;
		case 'M':
			{
				sprintf(s, "%s*.MML", syscfg.datadir);
				bFound = fnd.open(s, 0);
				if (bFound)
				{
					nl();
					GetSession()->bout << "No mailing lists available.\r\n\n";
					break;
				}
				nl();
				GetSession()->bout << "Available mailing lists:\r\n\n";
				while (bFound)
				{
					strcpy(s, fnd.GetFileName());
					sss = strchr(s, '.');
					if (sss)
					{
						*sss = 0;
					}
					GetSession()->bout << s;
					nl();

					bFound = fnd.next();
				}

				nl();
				GetSession()->bout << "|#2Which? ";
				input(s, 8);

				WFile fileMailList( syscfg.datadir, s );
				if ( !fileMailList.Open( WFile::modeBinary | WFile::modeReadOnly ) )
				{
					nl();
					GetSession()->bout << "Unknown mailing list.\r\n\n";
				}
				else
				{
					i1 = fileMailList.GetLength();
					mml_s = static_cast<char *>( BbsAllocA( i1 + 10L ) );
					fileMailList.Read( mml_s, i1 );
					mml_s[i1] = '\n';
					mml_s[i1 + 1] = 0;
					fileMailList.Close();
					mml_started = 0;
					add_list(nUserNumber, &numu, MAX_LIST, so());
					if (mml_s)
					{
						BbsFreeMemory(mml_s);
						mml_s = NULL;
					}
				}
			}
			break;
		case 'E':
			if (!numu)
			{
				nl();
				GetSession()->bout << "Need to specify some users first - use A or M\r\n\n";
			}
			else
			{
				multimail(nUserNumber, numu);
				done = true;
			}
			break;
		case 'D':
			if (numu)
			{
				nl();
				GetSession()->bout << "|#2Delete which? ";
				input(s, 2);
				i = atoi(s);
				if ((i > 0) && (i <= numu))
				{
					--numu;
					for (i1 = i - 1; i1 < numu; i1++)
					{
						nUserNumber[i1] = nUserNumber[i1 + 1];
					}
				}
			}
			break;
		case 'L':
			for (i = 0; i < numu; i++)
			{
                WUser user;
                GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber[i] );
				GetSession()->bout << i + 1 << ". " << user.GetUserNameAndNumber( nUserNumber[i] ) << wwiv::endl;
			}
			break;
		}
        CheckForHangup();
	} while ( !done && !hangup );
}
