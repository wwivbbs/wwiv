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


//////////////////////////////////////////////////////////////////////////////
//
// Implementation
//
//
//


void normalupload(int dn)
{
    uploadsrec u, u1;
    memset( &u, 0, sizeof( uploadsrec ) );
    memset( &u1, 0, sizeof( uploadsrec ) );

    int ok = 1;

    dliscan1( dn );
    directoryrec d = directories[dn];
    if (sess->numf >= d.maxfiles)
    {
        nl( 3 );
        sess->bout << "This directory is currently full.\r\n\n";
        return;
    }
    if ((d.mask & mask_no_uploads) && (!dcs()))
    {
        nl( 2 );
        sess->bout << "Uploads are not allowed to this directory.\r\n\n";
        return;
    }
    sess->bout << "|#9Filename: ";
    char szInputFileName[ MAX_PATH ];
    input( szInputFileName, 12 );
    if (!okfn( szInputFileName ))
    {
        szInputFileName[0] = '\0';
    }
    else
    {
        if ( !is_uploadable( szInputFileName ) )
        {
            if (so())
            {
                nl();
                sess->bout << "|#5In filename database - add anyway? ";
                if (!yesno())
                {
                    szInputFileName[0] = '\0';
                }
            }
            else
            {
                szInputFileName[0] = '\0';
                nl();
                sess->bout << "|12File either already here or unwanted.\r\n";
            }
        }
    }
    align( szInputFileName );
    if ( strchr( szInputFileName, '?' ) )
    {
        return;
    }
    if (d.mask & mask_archive)
    {
        ok = 0;
        std::string supportedExtensions;
        for (int k = 0; k < MAX_ARCS; k++)
        {
            if (arcs[k].extension[0] && arcs[k].extension[0] != ' ')
            {
                if ( !supportedExtensions.empty() )
                {
                    supportedExtensions += ", ";
                }
                supportedExtensions += arcs[k].extension;
                if ( wwiv::stringUtils::IsEquals( szInputFileName + 9, arcs[k].extension ) )
                {
                    ok = 1;
                }
            }
        }
        if ( !ok )
        {
            nl();
            sess->bout << "Sorry, all uploads to this dir must be archived.  Supported types are:\r\n" << supportedExtensions << "\r\n\n";
            return;
        }
    }
    strcpy( u.filename, szInputFileName );
    u.ownerusr = static_cast<unsigned short>( sess->usernum );
    u.ownersys = 0;
    u.numdloads = 0;
    u.filetype = 0;
    u.mask = 0;
    strcpy( u.upby, sess->thisuser.GetUserNameAndNumber( sess->usernum ) );
    strcpy( u.date, date() );
    nl();
    ok = 1;
    bool xfer = true;
    if (check_batch_queue(u.filename))
    {
        ok = 0;
        nl();
        sess->bout << "That file is already in the batch queue.\r\n\n";
    }
    else
    {
        if ( !wwiv::stringUtils::IsEquals( szInputFileName, "        .   " ) )
        {
            sess->bout << "|#5Upload '" << szInputFileName << "' to " << d.name << "? ";
        }
        else
        {
            ok = 0;
        }
    }
    if ( ok && yesno() )
    {
        WFile file( d.path, szInputFileName );
        if ( file.Exists() )
        {
            if ( dcs() )
            {
                xfer = false;
                nl( 2 );
                sess->bout << "File already exists.\r\n|#5Add to database anyway? ";
                if ( !yesno() )
                {
                    ok = 0;
                }
            }
            else
            {
                nl( 2 );
                sess->bout << "That file is already here.\r\n\n";
                ok = 0;
            }
        }
        else if (!incom)
        {
            nl();
            sess->bout << "File isn't already there.\r\nCan't upload locally.\r\n\n";
            ok = 0;
        }
        if ((d.mask & mask_PD) && ok )
        {
            nl();
            sess->bout << "|#5Is this program PD/Shareware? ";
            if (!yesno())
            {
                nl();
                sess->bout << "This directory is for Public Domain/\r\nShareware programs ONLY.  Please do not\r\n";
                sess->bout << "upload other programs.  If you have\r\ntrouble with this policy, please contact\r\n";
                sess->bout << "the sysop.\r\n\n";
                char szBuffer[ 255 ];
                sprintf( szBuffer , "Wanted to upload \"%s\"", u.filename );
                add_ass( 5, szBuffer );
                ok = 0;
            }
            else
            {
                u.mask = mask_PD;
            }
        }
        if ( ok && !GetApplication()->HasConfigFlag( OP_FLAGS_FAST_SEARCH ) )
        {
            nl();
            sess->bout << "Checking for same file in other directories...\r\n\n";
            int nLastLineLength = 0;
            for ( int i = 0; i < sess->num_dirs && udir[i].subnum != -1; i++ )
            {
                std::string buffer = "Scanning ";
                buffer += directories[udir[i].subnum].name;
                int nBufferLen = buffer.length();
                for ( int i3 = nBufferLen; i3 < nLastLineLength; i3++ )
                {
                    buffer += " ";
                }
                nLastLineLength = nBufferLen;
                sess->bout << buffer << "\r";
                dliscan1( udir[i].subnum );
                int i1 = recno( u.filename );
                if ( i1 >= 0 )
                {
                    nl();
					sess->bout << "Same file found on " << directories[udir[i].subnum].name << wwiv::endl;
                    if (dcs())
                    {
                        nl();
                        sess->bout << "|#5Upload anyway? ";
                        if (!yesno())
                        {
                            ok = 0;
                            break;
                        }
                        nl();
                    }
                    else
                    {
                        ok = 0;
                        break;
                    }
                }
            }
            std::string filler = charstr( nLastLineLength, SPACE );
            sess->bout << filler << "\r";
            if (ok)
            {
                dliscan1(dn);
            }
            nl();
        }
        if (ok)
        {
            nl();
			sess->bout << "Please enter a one line description.\r\n:";
            inputl(u.description, 58);
            nl();
            char *pszExtendedDescription = NULL;
            modify_extended_description( &pszExtendedDescription, directories[dn].name, u.filename );
            if ( pszExtendedDescription )
            {
                add_extended_description( u.filename, pszExtendedDescription );
                u.mask |= mask_extended;
                BbsFreeMemory( pszExtendedDescription );
            }
            nl();
            char szReceiveFileName[ MAX_PATH ];
            memset( szReceiveFileName, 0, sizeof( szReceiveFileName ) );
            if ( xfer )
            {
                write_inst(INST_LOC_UPLOAD, udir[sess->GetCurrentFileArea()].subnum, INST_FLAGS_ONLINE);
                double ti = timer();
                receive_file( szReceiveFileName, &ok, reinterpret_cast<char*>( &u.filetype ) , u.filename, dn );
                ti = timer() - ti;
                if (ti < 0)
                {
                    ti += SECONDS_PER_HOUR_FLOAT * HOURS_PER_DAY_FLOAT;
                }
                sess->thisuser.SetExtraTime( sess->thisuser.GetExtraTime() + static_cast<float>( ti ) );
            }
            if (ok)
            {
				WFile file( szReceiveFileName );
                if (ok == 1)
                {
					if ( !file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
                    {
                        ok = 0;
                        nl( 2 );
                        sess->bout << "OS error - File not found.\r\n\n";
                        if (u.mask & mask_extended)
                        {
                            delete_extended_description(u.filename);
                        }
                    }
                    if (ok && syscfg.upload_c[0])
                    {
						file.Close();
                        sess->bout << "Please wait...\r\n";
                        if ( !check_ul_event( dn, &u ) )
                        {
                            if ( u.mask & mask_extended )
                            {
                                delete_extended_description( u.filename );
                            }
                            ok = 0;
                        }
                        else
                        {
                            file.Open( WFile::modeBinary | WFile::modeReadOnly );
                        }
                    }
                }
                if (ok)
                {
                    if (ok == 1)
                    {
						long lFileLength = file.GetLength();
                        u.numbytes = lFileLength;
						file.Close();
                        sess->thisuser.SetFilesUploaded( sess->thisuser.GetFilesUploaded() + 1 );
                        modify_database( u.filename, true );
                        sess->thisuser.SetUploadK( sess->thisuser.GetUploadK() + bytes_to_k( u.numbytes ) );

                        get_file_idz(&u, dn);
                    }
                    else
                    {
                        u.numbytes = 0;
                    }
                    long lCurrentTime;
                    time( &lCurrentTime );
                    u.daten = lCurrentTime;
					WFile fileDownload( g_szDownloadFileName );
					fileDownload.Open( WFile::modeBinary|WFile::modeCreateFile|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite );
                    for (int j = sess->numf; j >= 1; j--)
                    {
                        FileAreaSetRecord( fileDownload, j );
						fileDownload.Read( &u1, sizeof( uploadsrec ) );
                        FileAreaSetRecord( fileDownload, j + 1 );
						fileDownload.Write( &u1, sizeof( uploadsrec ) );
                    }
                    FileAreaSetRecord( fileDownload, 1 );
					fileDownload.Write( &u, sizeof( uploadsrec ) );
                    ++sess->numf;
                    FileAreaSetRecord( fileDownload, 0 );
					fileDownload.Read( &u1, sizeof( uploadsrec ) );
                    u1.numbytes = sess->numf;
                    u1.daten = lCurrentTime;
                    sess->m_DirectoryDateCache[dn] = lCurrentTime;
                    FileAreaSetRecord( fileDownload, 0 );
					fileDownload.Write( &u1, sizeof( uploadsrec ) );
					fileDownload.Close();
                    if (ok == 1)
                    {
                        GetApplication()->GetStatusManager()->Lock();
                        ++status.uptoday;
                        ++status.filechange[filechange_upload];
                        GetApplication()->GetStatusManager()->Write();
                        sysoplogf( "+ \"%s\" uploaded on %s", u.filename, directories[dn].name);
                        nl( 2 );
                        bprintf( "File uploaded.\r\n\nYour ratio is now: %-6.3f\r\n", ratio() );
                        nl( 2 );
                        if ( sess->IsUserOnline() )
                        {
                            GetApplication()->GetLocalIO()->UpdateTopScreen();
                        }
                    }
                }
            }
            else
            {
                nl( 2 );
                sess->bout << "File transmission aborted.\r\n\n";
                if (u.mask & mask_extended)
                {
                    delete_extended_description(u.filename);
                }
            }
        }
    }
}

