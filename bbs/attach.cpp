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



void attach_file(int mode)
{
    bool bFound;
    char szFullPathName[ MAX_PATH ], szNewFileName[ MAX_PATH], szFileToAttach[ MAX_PATH ];
    WUser u;
    filestatusrec fsr;

    char szAttachFileName[ MAX_PATH ];
    sprintf(szAttachFileName, "%s%s", syscfg.datadir, ATTACH_DAT);

    nl();
    bool bDirectionForward = true;
	WFile *pFileEmail = OpenEmailFile( true );
	WWIV_ASSERT( pFileEmail );
	if ( !pFileEmail->IsOpen() )
    {
        GetSession()->bout << "\r\nNo mail.\r\n";
		pFileEmail->Close();
		delete pFileEmail;
        return;
    }
	int max = static_cast<int>( pFileEmail->GetLength() / sizeof( mailrec ) );
    int cur = ( bDirectionForward ) ? max - 1 : 0;

    int ok = 0;
    bool done = false;
    do
    {
        mailrec m;
		pFileEmail->Seek( cur * sizeof(mailrec), WFile::seekBegin );
		pFileEmail->Read( &m, sizeof( mailrec ) );
        while ( ( m.fromsys != 0 || m.fromuser != GetSession()->usernum || m.touser == 0 ) &&
                cur < max && cur >= 0 )
        {
            if ( bDirectionForward )
            {
                --cur;
            }
            else
            {
                ++cur;
            }
            if ( cur < max && cur >= 0 )
            {
				pFileEmail->Seek( static_cast<long>( cur ) * static_cast<long>( sizeof( mailrec ) ), WFile::seekBegin );
				pFileEmail->Read( &m, sizeof( mailrec ) );
            }
        }
        if ( m.fromsys != 0 || m.fromuser != GetSession()->usernum || m.touser == 0 || cur >= max || cur < 0 )
        {
            done = true;
        }
        else
        {
            bool done1 = false;
            do
            {
                done1 = false;
                nl();
                if ( m.tosys == 0 )
                {
                    char szBuffer[ 255 ];
                    GetApplication()->GetUserManager()->ReadUser( &u, m.touser );
                    GetSession()->bout << "|#1  To|#7: |#2";
                    strcpy( szBuffer, u.GetUserNameAndNumber( m.touser ) );
                    if ( ( m.anony & (anony_receiver | anony_receiver_pp | anony_receiver_da ) ) &&
                        ( getslrec( GetSession()->GetEffectiveSl() ).ability & ability_read_email_anony ) == 0 )
                    {
                        strcpy(szBuffer, ">UNKNOWN<");
                    }
                    GetSession()->bout << szBuffer;
					nl();
                }
                else
                {
					GetSession()->bout << "|#1To|#7: |#2User " << m.tosys << " System " << m.touser << wwiv::endl;
                }
				GetSession()->bout << "|#1Subj|#7: |#2" << m.title << wwiv::endl;
                time_t tTimeNow = time( NULL );
				int nDaysAgo = static_cast<int>( ( tTimeNow - m.daten ) / HOURS_PER_DAY_FLOAT / SECONDS_PER_HOUR_FLOAT );
				GetSession()->bout << "|#1Sent|#7: |#2 " << nDaysAgo << " days ago" << wwiv::endl;
                if (m.status & status_file)
                {
					WFile fileAttach( szAttachFileName );
					if ( fileAttach.Open( WFile::modeBinary | WFile::modeReadOnly, WFile::shareUnknown, WFile::permReadWrite ) )
                    {
                        bFound = false;
						long lNumRead = fileAttach.Read( &fsr, sizeof( fsr ) );
                        while ( lNumRead > 0 && !bFound )
                        {
                            if (m.daten == static_cast<unsigned long>( fsr.id ))
                            {
								GetSession()->bout << "|#1Filename|#0.... |#2" << fsr.filename << " (" << fsr.numbytes << " bytes)|#0";
                                bFound = true;
                            }
                            if (!bFound)
                            {
								lNumRead = fileAttach.Read( &fsr, sizeof( fsr ) );
                            }
                        }
                        if (!bFound)
                        {
                            GetSession()->bout << "|#1Filename|#0.... |#2Unknown or missing|#0\r\n";
                        }
						fileAttach.Close();
                    }
                    else
                    {
                        GetSession()->bout << "|#1Filename|#0.... |#2Unknown or missing|#0\r\n";
                    }
                }
                nl();
                char ch = 0;
                if (mode == 0)
                {
                    GetSession()->bout << "|#9(R)ead, (A)ttach, (N)ext, (Q)uit : ";
                    ch = onek("QRAN");
                }
                else
                {
                    GetSession()->bout << "|#9(R)ead, (A)ttach, (Q)uit : ";
                    ch = onek("QRA");
                }
                switch (ch)
                {
                case 'Q':
                    done1 = true;
                    done = true;
                    break;
                case 'A':
                    {
                        bool newname    = false;
                        bool done2      = false;
                        if (m.status & status_file)
                        {
                            GetSession()->bout << "|#6File already attached, (D)elete, (O)verwrite, or (Q)uit? : ";
                            char ch1 = onek("QDO");
                            switch (ch1)
                            {
                            case 'Q':
                                done2 = true;
                                break;
                            case 'D':
                            case 'O':
                                {
                                    m.status ^= status_file;
									pFileEmail->Seek( static_cast<long>( sizeof( mailrec ) ) * -1L, WFile::seekCurrent );
									pFileEmail->Write( &m, sizeof( mailrec ) );
                                    WFile attachFile( szAttachFileName );
                                    if ( attachFile.Open( WFile::modeReadWrite | WFile::modeBinary ) )
                                    {
                                        bFound = false;
                                        long lNumRead = attachFile.Read( &fsr, sizeof( fsr ) );
                                        while ( lNumRead > 0 && !bFound )
                                        {
                                            if ( m.daten == static_cast<unsigned long>( fsr.id ) )
                                            {
                                                fsr.id = 0;
                                                WFile::Remove( g_szAttachmentDirectory, fsr.filename );
                                                attachFile.Seek( static_cast<long>( sizeof( filestatusrec ) ) * -1L, WFile::seekCurrent );
                                                attachFile.Write( &fsr, sizeof( fsr ) );
                                            }
                                            if (!bFound)
                                            {
                                                lNumRead = attachFile.Read( &fsr, sizeof( fsr ) );
                                            }
                                        }
                                        attachFile.Close();
                                        GetSession()->bout << "File attachment removed.\r\n";
                                    }
                                    if (ch1 == 'D')
                                    {
                                        done2 = true;
                                    }
                                    break;
                                }
                            }
                        }
                        if (freek1(g_szAttachmentDirectory) < 500)
                        {
                            GetSession()->bout << "Not enough free space to attach a file.\r\n";
                        }
                        else
                        {
                            if (!done2)
                            {
                                bool bRemoteUpload = false;
                                bFound = false;
                                nl();
                                if (so())
                                {
                                    if (incom)
                                    {
                                        GetSession()->bout << "|#5Upload from remote? ";
                                        if (yesno())
                                        {
                                            bRemoteUpload = true;
                                        }
                                    }
                                    if (!bRemoteUpload)
                                    {
                                        GetSession()->bout << "|#5Path/filename (wildcards okay) : \r\n";
                                        input( szFileToAttach, 35, true );
                                        if (szFileToAttach[0])
                                        {
                                            nl();
                                            if (strchr(szFileToAttach, '*') != NULL || strchr(szFileToAttach, '?') != NULL)
                                            {
                                                strcpy(szFileToAttach, get_wildlist(szFileToAttach));
                                            }
                                            if (!WFile::Exists(szFileToAttach))
                                            {
                                                bFound = true;
                                            }
                                            if (!okfn( stripfn( szFileToAttach ) ) || strchr( szFileToAttach, '?' ) )
                                            {
                                                bFound = true;
                                            }
                                        }
                                        if ( !bFound && szFileToAttach[0] )
                                        {
                                            sprintf(szFullPathName, "%s%s", g_szAttachmentDirectory, stripfn(szFileToAttach));
                                            nl();
											GetSession()->bout << "|#5" << szFileToAttach << "? ";
                                            if ( !yesno() )
                                            {
                                                bFound = true;
                                            }
                                        }
                                    }
                                }
                                if ( !so() || bRemoteUpload )
                                {
                                    GetSession()->bout << "|#2Filename: ";
                                    input( szFileToAttach, 12, true );
                                    sprintf(szFullPathName, "%s%s", g_szAttachmentDirectory, szFileToAttach);
                                    if ( !okfn(szFileToAttach) || strchr(szFileToAttach, '?') )
                                    {
                                        bFound = true;
                                    }
                                }
                                if (WFile::Exists(szFullPathName))
                                {
                                    GetSession()->bout << "Target file exists.\r\n";
                                    bool done3 = false;
                                    do
                                    {
                                        bFound = true;
                                        GetSession()->bout << "|#5New name? ";
                                        if (yesno())
                                        {
                                            GetSession()->bout << "|#5Filename: ";
                                            input( szNewFileName, 12, true );
                                            sprintf(szFullPathName, "%s%s", g_szAttachmentDirectory, szNewFileName);
                                            if ( okfn(szNewFileName) && !strchr(szNewFileName, '?') && !WFile::Exists(szFullPathName) )
                                            {
                                                bFound   = false;
                                                done3   = true;
                                                newname = true;
                                            }
                                            else
                                            {
                                                GetSession()->bout << "Try Again.\r\n";
                                            }
                                        }
                                        else
                                        {
                                            done3 = true;
                                        }
                                    } while ( !done3 && !hangup );
                                }
								WFile fileAttach( szAttachFileName );
								if ( fileAttach.Open( WFile::modeBinary|WFile::modeReadOnly, WFile::shareUnknown, WFile::permReadWrite ) )
                                {
									long lNumRead = fileAttach.Read( &fsr, sizeof( fsr ) );
                                    while ( lNumRead > 0 && !bFound )
                                    {
                                        if ( m.daten == static_cast<unsigned long>( fsr.id ) )
                                        {
                                            bFound = true;
                                        }
                                        else
                                        {
											lNumRead = fileAttach.Read( &fsr, sizeof( fsr ) );
                                        }
                                    }
									fileAttach.Close();
                                }
                                if (bFound)
                                {
                                    GetSession()->bout << "File already exists or invalid filename.\r\n";
                                }
                                else
                                {
                                    if ( so() && !bRemoteUpload )
                                    {
                                        // Copy file s to szFullPathName.
                                        if (!WFile::CopyFile(szFileToAttach, szFullPathName))
                                        {
                                            GetSession()->bout << "done.\r\n";
                                            ok = 1;
                                        }
                                        else
                                        {
                                            GetSession()->bout << "\r\n|#6Error in copy.\r\n";
                                            getkey();
                                        }
                                    }
                                    else
                                    {
                                        sprintf(szFullPathName, "%s%s", g_szAttachmentDirectory, szFileToAttach);
                                        unsigned char u_filetype;
                                        receive_file( szFullPathName, &ok, reinterpret_cast<char*>( &u_filetype ), "", 0 );
                                    }
                                    if (ok)
                                    {
                                        WFile attachmentFile( szFullPathName );
                                        if ( !attachmentFile.Open( WFile::modeReadOnly | WFile::modeBinary ) )
                                        {
                                            ok = 0;
                                            GetSession()->bout << "\r\n\nDOS error - File not bFound.\r\n\n";
                                        }
                                        else
                                        {
                                            fsr.numbytes = attachmentFile.GetLength();
                                            attachmentFile.Close();
                                            if (newname)
                                            {
                                                strcpy(fsr.filename, stripfn(szNewFileName));
                                            }
                                            else
                                            {
                                                strcpy(fsr.filename, stripfn(szFileToAttach));
                                            }
                                            fsr.id = m.daten;
											GetSession()->bout << "|#5Attach " << fsr.filename << " (" << fsr.numbytes << " bytes) to Email? ";
                                            if ( yesno() )
                                            {
                                                m.status ^= status_file;
												pFileEmail->Seek( static_cast<long>( sizeof( mailrec ) ) * -1L, WFile::seekCurrent );
												pFileEmail->Write( &m, sizeof( mailrec ) );
                                                WFile attachFile( szAttachFileName );
                                                if ( !attachFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite ) )
                                                {
                                                    GetSession()->bout << "Could not write attachment data.\r\n";
                                                    m.status ^= status_file;
													pFileEmail->Seek( static_cast<long>( sizeof( mailrec ) ) * -1L, WFile::seekCurrent );
													pFileEmail->Write( &m, sizeof( mailrec ) );
                                                }
                                                else
                                                {
                                                    filestatusrec fsr1;
                                                    fsr1.id = 1;
                                                    long lNumRead = 0;
                                                    do
                                                    {
                                                        lNumRead = attachFile.Read( &fsr1, sizeof( filestatusrec ) );
                                                    } while ( lNumRead > 0 && fsr1.id != 0 );

                                                    if (fsr1.id == 0)
                                                    {
                                                        attachFile.Seek( static_cast<long>( sizeof(filestatusrec) ) * -1L, WFile::seekCurrent );
                                                    }
                                                    else
                                                    {
                                                        attachFile.Seek( 0L, WFile::seekEnd );
                                                    }
                                                    attachFile.Write( &fsr, sizeof( filestatusrec ) );
                                                    attachFile.Close();
                                                    char szLogLine[ 255 ];
                                                    sprintf( szLogLine, "Attached %s (%ld bytes) in message to %s",
                                                             fsr.filename, fsr.numbytes, u.GetUserNameAndNumber( m.touser ) );
                                                    GetSession()->bout << "File attached.\r\n" ;
                                                    sysoplog( szLogLine );
                                                }
                                            }
                                            else
                                            {
                                                WFile::Remove( g_szAttachmentDirectory, fsr.filename );
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;
                case 'N':
                    done1 = true;
                    if ( bDirectionForward )
                    {
                        --cur;
                    }
                    else
                    {
                        ++cur;
                    }
                    if ( cur >= max || cur < 0 )
                    {
                        done = true;
                    }
                    break;
                case 'R':
                    {
                        nl( 2 );
                        GetSession()->bout << "Title: " << m.title;
                        bool next;
                        read_message1(&m.msg, static_cast< char >( m.anony & 0x0f ), false, &next, "email", 0, 0);
                        if (m.status & status_file)
                        {
							WFile fileAttach( syscfg.datadir, ATTACH_DAT );
							if ( fileAttach.Open( WFile::modeReadOnly|WFile::modeBinary, WFile::shareUnknown, WFile::permReadWrite ) )
                            {
                                bFound = false;
								long lNumRead = fileAttach.Read( &fsr, sizeof( fsr ) );
                                while ( lNumRead > 0 && !bFound )
                                {
                                    if ( m.daten == static_cast<unsigned long>( fsr.id ) )
                                    {
										GetSession()->bout << "Attached file: " << fsr.filename << " (" << fsr.numbytes << " bytes).";
                                        nl();
                                        bFound = true;
                                    }
                                    if (!bFound)
                                    {
										lNumRead = fileAttach.Read( &fsr, sizeof( fsr ) );
                                    }
                                }
                                if (!bFound)
                                {
                                    GetSession()->bout << "File attached but attachment data missing.  Alert sysop!\r\n";
                                }
								fileAttach.Close();
                            }
                            else
                            {
                                GetSession()->bout << "File attached but attachment data missing.  Alert sysop!\r\n";
                            }
                        }
                    }
                    break;
                }
            } while ( !hangup && !done1 );
        }
    } while ( !done && !hangup );
}

