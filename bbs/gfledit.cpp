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


char *gfiledata( int nSectionNum, char *pBuffer );


char *gfiledata( int nSectionNum, char *pBuffer )
{
	gfiledirrec r = gfilesec[ nSectionNum ];
	char x = SPACE;
	if (r.ar != 0)
	{
		for (int i = 0; i < 16; i++)
		{
			if ((1 << i) & r.ar)
			{
				x = static_cast<char>( 'A' + i );
			}
		}
	}
	sprintf( pBuffer, "|#2%2d |13%1c  |#1%-40s  |#2%-8s |#9%-3d %-3d %-3d",
			 nSectionNum, x, stripcolors(r.name), r.filename, r.sl, r.age, r.maxfiles );
	return pBuffer;
}


void showsec()
{
	char szBuffer[255];

	ClearScreen();
	bool abort = false;
	pla("|#2NN AR Name                                      FN       SL  AGE MAX", &abort);
	pla("|#7-- == ----------------------------------------  ======== --- === ---", &abort);
	for (int i = 0; (i < sess->num_sec) && (!abort); i++)
	{
		pla( gfiledata( i, szBuffer ), &abort );
	}
}


bool exist_dir(const char *pszDirectoryName)
{
	WWIV_ASSERT( pszDirectoryName );
	bool ok = false;

	WWIV_ChangeDirTo(syscfg.gfilesdir);
	if ( chdir( pszDirectoryName ) )
	{
		ok = false;
	}
	else
	{
		ok = true;
	}
	GetApplication()->CdHome();
	return ok;
}


char* GetArString( gfiledirrec r, char* pszBuffer )
{
    char szBuffer[ 81 ];
    strcpy(szBuffer, "None.");
    if (r.ar != 0)
    {
	    for (int i = 0; i < 16; i++)
	    {
		    if ((1 << i) & r.ar)
		    {
			    szBuffer[0] = static_cast<char>( 'A' + i );
		    }
	    }
	    szBuffer[1] = 0;
    }
    strcpy( pszBuffer, szBuffer );
    return pszBuffer;
}


void modify_sec(int n)
{
	char s[81];
	char szSubNum[255];

	gfiledirrec r = gfilesec[n];
	bool done = false;
	do
	{
		ClearScreen();
		sprintf( szSubNum, "|B1|15Editing G-File Area # %d", n );
		bprintf( "%-85s", szSubNum );
        ansic( 0 );
		nl( 2 );
		sess->bout << "|#9A) Name       : |#2" << r.name << wwiv::endl;
		sess->bout << "|#9B) Filename   : |#2" << r.filename << wwiv::endl;
		sess->bout << "|#9C) SL         : |#2" << static_cast<int>( r.sl ) << wwiv::endl;
		sess->bout << "|#9D) Min. Age   : |#2" << static_cast<int>( r.age ) << wwiv::endl;
		sess->bout << "|#9E) Max Files  : |#2" << r.maxfiles << wwiv::endl;
		sess->bout << "|#9F) AR         : |#2" << GetArString( r, s ) << wwiv::endl;

        nl();
		sess->bout << "|#7(|#2Q|#7=|#1Quit|#7) Which (|#1A|#7-|#1F,|#1[|#7,|#1]|#7) : ";
		char ch = onek( "QABCDEF[]", true );
		switch (ch)
		{
		case 'Q':
			done = true;
			break;
		case '[':
			gfilesec[n] = r;
			if (--n < 0)
			{
				n = sess->num_sec - 1;
			}
			r = gfilesec[n];
			break;
		case ']':
			gfilesec[n] = r;
			if (++n >= sess->num_sec)
			{
				n = 0;
			}
			r = gfilesec[n];
			break;
		case 'A':
			nl();
			sess->bout << "|#2New name? ";
			inputl(s, 40);
			if (s[0])
			{
				strcpy(r.name, s);
			}
			break;
		case 'B':
			nl();
			if (exist_dir(r.filename))
			{
				sess->bout << "\r\nThere is currently a directory for this g-file section.\r\n";
				sess->bout << "If you change the filename, the directory will still be there.\r\n\n";
			}
			nl();
			sess->bout << "|#2New filename? ";
			input(s, 8);
			if ( ( s[0] != 0 ) && ( strchr(s, '.') == 0 ) )
			{
				strcpy(r.filename, s);
				if (!exist_dir(r.filename))
				{
					nl();
					sess->bout << "|#5Create directory for this section? ";
					if (yesno())
					{
						WWIV_ChangeDirTo(syscfg.gfilesdir);
						mkdir(r.filename);
						GetApplication()->CdHome();
					}
					else
					{
						sess->bout << "\r\nYou will have to create the directory manually, then.\r\n\n";
					}
				}
				else
				{
					sess->bout << "\r\nA directory already exists under this filename.\r\n\n";
				}
				pausescr();
			}
			break;
		case 'C':
            {
			    nl();
			    sess->bout << "|#2New SL? ";
			    input(s, 3);
			    int i = atoi(s);
			    if ( i >= 0 && i < 256 && s[0] )
			    {
				    r.sl = static_cast<unsigned char>( i );
			    }
            }
			break;
		case 'D':
            {
			    nl();
			    sess->bout << "|#2New Min Age? ";
			    input(s, 3);
			    int i = atoi(s);
			    if ((i >= 0) && (i < 128) && (s[0]))
			    {
				    r.age = static_cast<unsigned char>( i );
			    }
            }
			break;
		case 'E':
            {
			    nl();
				sess->bout << "|#1Max 99 files/section.\r\n|#2New max files? ";
			    input(s, 3);
			    int i = atoi(s);
			    if ((i >= 0) && (i < 99) && (s[0]))
			    {
				    r.maxfiles = static_cast<unsigned short>( i );
			    }
            }
			break;
		case 'F':
			nl();
			sess->bout << "|#2New AR (<SPC>=None) ? ";
			char ch2 = onek("ABCDEFGHIJKLMNOP ");
			if (ch2 == SPACE)
			{
				r.ar = 0;
			}
			else
			{
				r.ar = 1 << (ch2 - 'A');
			}
			break;
		}
  } while ( !done && !hangup );
  gfilesec[n] = r;
}


void insert_sec(int n)
{
	gfiledirrec r;

	for (int i = sess->num_sec - 1; i >= n; i--)
	{
		gfilesec[i + 1] = gfilesec[i];
	}
	strcpy(r.name, "** NEW SECTION **");
	strcpy(r.filename, "NONAME");
	r.sl		= 10;
	r.age		= 0;
	r.maxfiles	= 99;
	r.ar		= 0;
	gfilesec[n] = r;
	++sess->num_sec;
	modify_sec( n );
}


void delete_sec(int n)
{
	for (int i = n; i < sess->num_sec; i++)
	{
		gfilesec[i] = gfilesec[i + 1];
	}
	--sess->num_sec;
}


void gfileedit()
{
	int i;
	char s[81];

	if (!ValidateSysopPassword())
	{
		return;
	}
	showsec();
	bool done = false;
	do
	{
		nl();
		sess->bout << "|#2G-files: D:elete, I:nsert, M:odify, Q:uit, ? : ";
		char ch = onek("QDIM?");
		switch (ch)
		{
		case '?':
			showsec();
			break;
		case 'Q':
			done = true;
			break;
		case 'M':
			nl();
			sess->bout << "|#2Section number? ";
			input(s, 2);
			i = atoi(s);
			if ((s[0] != 0) && (i >= 0) && (i < sess->num_sec))
			{
				modify_sec(i);
			}
			break;
		case 'I':
			if (sess->num_sec < sess->max_gfilesec)
			{
				nl();
				sess->bout << "|#2Insert before which section? ";
				input(s, 2);
				i = atoi(s);
				if ((s[0] != 0) && (i >= 0) && (i <= sess->num_sec))
				{
					insert_sec(i);
				}
			}
			break;
		case 'D':
			nl();
			sess->bout << "|#2Delete which section? ";
			input(s, 2);
			i = atoi(s);
			if ((s[0] != 0) && (i >= 0) && (i < sess->num_sec))
			{
				nl();
                sess->bout << "|#5Delete " << gfilesec[i].name << "?";
				if (yesno())
				{
					delete_sec(i);
				}
			}
			break;
		}
	} while ( !done && !hangup );
    WFile gfileDat( syscfg.datadir, GFILE_DAT );
    gfileDat.Open( WFile::modeReadWrite |WFile::modeBinary | WFile::modeCreateFile | WFile::modeTruncate, WFile::shareUnknown, WFile::permReadWrite );
    gfileDat.Write( &gfilesec[0], sess->num_sec * sizeof( gfiledirrec ) );
    gfileDat.Close();
}


bool fill_sec(int sn)
{
	int nf = 0, i, i1, n1;
	char s[81], s1[81];
	WFindFile fnd;
	bool bFound = false;

	gfilerec *g = read_sec(sn, &n1);
	sprintf(s1, "%s%s%c*.*", syscfg.gfilesdir, gfilesec[sn].filename, WWIV_FILE_SEPERATOR_CHAR);
	bFound = fnd.open(s1, 0);
	bool ok = true;
	int chd = 0;
	while ((bFound) && (!hangup) && (nf < gfilesec[sn].maxfiles) && (ok))
	{
		strcpy(s, fnd.GetFileName());
		align(s);
		i = 1;
		for (i1 = 0; i1 < nf; i1++)
		{
			if (compare(s, g[i1].filename))
			{
				i = 0;
			}
		}
		if (i)
		{
            sess->bout << "|#2" << s << " : ";
			inputl(s1, 60);
			if (s1[0])
			{
				chd = 1;
				i = 0;
                while ( wwiv::stringUtils::StringCompare(s1, g[i].description) > 0 && i < nf )
				{
					++i;
				}
				for (i1 = nf; i1 > i; i1--)
				{
					g[i1] = g[i1 - 1];
				}
				++nf;
				gfilerec g1;
				strcpy(g1.filename, s);
				strcpy(g1.description, s1);
				time(&(g1.daten));
				g[i] = g1;
			}
			else
			{
				ok = false;
			}
		}
		bFound = fnd.next();
	}
	if (!ok)
	{
		sess->bout << "|12Aborted.\r\n";
	}
	if (nf >= gfilesec[sn].maxfiles)
	{
		sess->bout << "Section full.\r\n";
	}
	if (chd)
	{
        char szFileName[ MAX_PATH ];
		sprintf( szFileName, "%s%s.gfl", syscfg.datadir, gfilesec[sn].filename );
        WFile gflFile( szFileName );
        gflFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile | WFile::modeTruncate, WFile::shareUnknown, WFile::permReadWrite );
        gflFile.Write( g, nf * sizeof( gfilerec ) );
        gflFile.Close();
		GetApplication()->GetStatusManager()->Lock();
		strcpy(status.gfiledate, date());
		GetApplication()->GetStatusManager()->Write();
	}
	BbsFreeMemory( g );
	return !ok;
}

void pack_all_subs( bool bFromCommandline )
{
	tmp_disable_pause( true );

    bool abort = false, next = false;
	int i = 0;
	while ( !hangup && !abort && i < sess->num_subs )
	{
		pack_sub(i);
		checka(&abort, &next);
		i++;
	}
	if (abort)
	{
		sess->bout << "|12Aborted.\r\n";
	}
	tmp_disable_pause( false );
}


void pack_sub(int si)
{
	if (iscan1(si, false))
	{
		if ( open_sub( true ) && subboards[si].storage_type == 2 )
		{

			char *sfn = subboards[si].filename;
			char *nfn = "PACKTMP$";

			char fn1[MAX_PATH], fn2[MAX_PATH];
			sprintf(fn1, "%s%s.dat", syscfg.msgsdir, sfn);
			sprintf(fn2, "%s%s.dat", syscfg.msgsdir, nfn);

			sess->bout << "\r\n|#7\xFE |#1Packing Message Area: |10" << subboards[si].name << wwiv::endl;

			for (int i = 1; i <= sess->GetNumMessagesInCurrentMessageArea(); i++)
			{
				if (i % 10 == 0)
				{
					sess->bout << i << "/" << sess->GetNumMessagesInCurrentMessageArea() << "\r";
				}
				postrec *p = get_post( i );
				if (p)
				{
					long lMessageSize;
					char *mt = readfile( &(p->msg), sfn, &lMessageSize );
					if (!mt)
					{
						mt = static_cast<char *>( BbsAllocA( 10 ) );
						WWIV_ASSERT(mt);
						if (mt)
						{
							strcpy(mt, "??");
							lMessageSize = 3;
						}
					}
					if (mt)
					{
						savefile(mt, lMessageSize, &(p->msg), nfn);
						write_post(i, p);
					}
				}
				sess->bout << i << "/" << sess->GetNumMessagesInCurrentMessageArea() << "\r";
			}

			WFile::Remove(fn1);
			WFile::Rename(fn2, fn1);

			close_sub();
			sess->bout << "|#7\xFE |#1Done Packing " << sess->GetNumMessagesInCurrentMessageArea() << " messages.                              \r\n";
		}
	}
}
