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


int try_to_ul_wh(char *pszFileName);
void t2u_error(char *pszFileName, char *msg);


//////////////////////////////////////////////////////////////////////////////
//
// Implementation
//
//
//


int try_to_ul(char *pszFileName)
{
    bool ac = false;

    if ( uconfsub[1].confnum != -1 && okconf( &sess->thisuser ) )
    {
        ac = true;
        tmp_disable_conf( true );
    }
    if ( !try_to_ul_wh( pszFileName ) )
	{
        if ( ac )
		{
            tmp_disable_conf( false );
		}
        return 0;  // success
    }

    char dest[MAX_PATH];
	sprintf(dest, "%sTRY2UL", syscfg.dloadsdir);
    if (chdir(dest))
	{
        GetApplication()->CdHome();  // get back to our bbs dir
		mkdir(dest);	// create the \DLOADS\TRY2UL dir
    }

	GetApplication()->CdHome();		// ensure we are in the correct directory

    sess->bout << "|#2Your file had problems, it is being moved to a special dir for sysop review\r\n";

    sysoplogf( "Failed to upload %s, moving to TRY2UL dir", pszFileName);

    char src[MAX_PATH];
    sprintf(src, "%s%s", syscfgovr.batchdir, pszFileName);
    sprintf(dest, "%sTRY2UL%c%s", syscfg.dloadsdir, WWIV_FILE_SEPERATOR_CHAR, pszFileName);

    if (WFile::Exists(dest))                          // this is iffy <sp?/who care I chooose to
    {
        WFile::Remove(dest);                           // remove duplicates in try2ul dir, so keep
    }
    // it clean and up to date
    copyfile(src, dest, true);                   // copy file from batch dir,to try2ul dir */

    if ( sess->IsUserOnline() )
    {
        GetApplication()->GetLocalIO()->UpdateTopScreen();
    }

    if (ac)
    {
        tmp_disable_conf( false );
    }

    return 1;                                 // return failure, removes ul k credits etc...
}


int try_to_ul_wh(char *pszFileName)
{
    directoryrec d;
    char s[101], s1[MAX_PATH], s2[MAX_PATH], *ss;
    int i, i1, i2, i3, i4, key, ok=0, dn = 0;
    uploadsrec u, u1;

    unalign(pszFileName);
    StringTrim(pszFileName);

    strcpy(s, pszFileName);
    if (!okfn(pszFileName))
    {
        t2u_error(pszFileName, "Bad filename");          // bad filename
        return 1;
    }
    ClearScreen();
    nl( 3 );

    bool done = false;
    if ( sess->thisuser.isRestrictionValidate() || sess->thisuser.isRestrictionUpload() ||
        ( syscfg.sysconfig & sysconfig_all_sysop ) )
    {
        dn = (syscfg.newuploads < sess->num_dirs) ? syscfg.newuploads : 0;
    }
    else
    {
        char temp[10];

        // The hangup check is below so uploads get uploaded even on hangup
		done = false;
        while (!done)
        {
            if (hangup)
            {
                if (syscfg.newuploads < sess->num_dirs)
                {
                    dn = syscfg.newuploads;
                }
                else
                {
                    dn = 0;
                }
                done = true;
            }
            else
            {
                // The WWIV_Delay used to be a wait_sec_or_hit( 1 )
                WWIV_Delay(500);
                sess->bout << "\r\nUpload " << pszFileName << " to which dir? <CR>=0 ?=List \r\n";
                input( temp, 5, true );
                StringTrim(temp);
                if (temp[0] == '?')
                {
                    dirlist( 1 );
                }
                else if (!temp[0])
                {
                    dn = 0;
                    done = true;
                }
                else
                {
                    int x = atoi(temp);
                    if (udir[x].subnum >= 0)
                    {
                        dliscan1(udir[x].subnum);
                        d = directories[dn];
                        if ((d.mask & mask_no_uploads) && (!dcs()))
                        {
                            sess->bout << "Can't upload there...\r\n";
                            pausescr();
                        }
                        else
                        {
                            dn = udir[x].subnum;
                            done = true;
                        }
                    }
                }
            }
        }
    }

    dliscan1(dn);
    d = directories[dn];
    if (sess->numf >= d.maxfiles)
    {
        t2u_error(pszFileName, "This directory is currently full.");
        return 1;
    }
    if ((d.mask & mask_no_uploads) && (!dcs()))
    {
        t2u_error(pszFileName, "Uploads are not allowed to this directory.");
        return 1;
    }
    if (!is_uploadable(s))
    {
        if (so())
        {
            nl();
            sess->bout << "|#5In filename database - add anyway? ";
            if (!yesno())
            {
                t2u_error(pszFileName, "|12File either already here or unwanted.");
                return 1;
            }
        }
        else
        {
            t2u_error(pszFileName, "|12File either already here or unwanted.");
            return 1;
        }
    }
    align(s);
    if (strchr(s, '?'))
    {
        t2u_error(pszFileName, "Contains wildcards");
        return 1;
    }
    if (d.mask & mask_archive)
    {
        ok = 0;
        s1[0] = '\0';
        for ( i = 0; i < MAX_ARCS; i++ )
        {
            if ( arcs[i].extension[0] && arcs[i].extension[0] != ' ' )
            {
                if ( s1[0] )
                {
                    strcat( s1, ", "  );
                }
                strcat( s1, arcs[i].extension );
                if ( wwiv::stringUtils::IsEquals( s + 9, arcs[i].extension ) )
                {
                    ok = 1;
                }
            }
        }
        if (!ok)
        {
            nl();
            sess->bout << "Sorry, all uploads to this directory must be archived.  Supported types are:\r\n";
            sess->bout << s1;
            nl( 2 );

            t2u_error(pszFileName, "Unsupported archive");
            return 1;
        }
    }
    strcpy(u.filename, s);
    u.ownerusr  = static_cast<unsigned short>( sess->usernum );
    u.ownersys  = 0;
    u.numdloads = 0;
    u.filetype  = 0;
    u.mask      = 0;
    strncpy( u.upby, sess->thisuser.GetUserNameAndNumber( sess->usernum ), sizeof( u.upby ) );
    u.upby[36]  = '\0';
    strcpy(u.date, date());

    sprintf(s1, "%s%s", d.path, s);
    if (WFile::Exists(s1))
    {
        if (dcs())
        {
            nl( 2 );
            sess->bout << "File already exists.\r\n|#5Add to database anyway? ";
            if (yesno() == 0)
            {
                t2u_error(pszFileName, "That file is already here.");
                return 1;
            }
        }
        else
        {
            t2u_error(pszFileName, "That file is already here.");
            return 1;
        }
    }
    if ( ok && (!GetApplication()->HasConfigFlag( OP_FLAGS_FAST_SEARCH ) ) )
    {
        nl();
        sess->bout << "Checking for same file in other directories...\r\n\n";
        i2 = 0;

        for (i = 0; (i < sess->num_dirs) && (udir[i].subnum != -1); i++)
        {
            strcpy(s1, "Scanning ");
            strcat(s1, directories[udir[i].subnum].name);

            for (i3 = i4 = strlen(s1); i3 < i2; i3++)
            {
                s1[i3] = ' ';
                s1[i3 + 1] = 0;
            }

            i2 = i4;
            sess->bout << s1;
            bputch('\r');

            dliscan1(udir[i].subnum);
            i1 = recno(u.filename);
            if (i1 >= 0)
            {
                nl();
				sess->bout << "Same file found on " << directories[udir[i].subnum].name << wwiv::endl;

                if (dcs())
                {
                    nl();
                    sess->bout << "|#5Upload anyway? ";
                    if (!yesno())
                    {
                        t2u_error(pszFileName, "That file is already here.");
                        return 1;
                    }
                    nl();
                }
                else
                {
                    t2u_error(pszFileName, "That file is already here.");
                    return 1;
                }
            }
        }


        for (i1 = 0; i1 < i2; i1++)
        {
            s1[i1] = ' ';
        }
        s1[i1] = '\0';
        sess->bout << s1 << "\r";

        dliscan1(dn);
        nl();
    }
    sprintf(s1, "%s%s", syscfgovr.batchdir, pszFileName);
    sprintf(s2, "%s%s", d.path, pszFileName);

    if (WFile::Exists(s2))
    {
        WFile::Remove(s2);
    }

	// s1 and s2 should remain set,they are used below
    movefile(s1, s2, true);
    strcpy(u.description, "NO DESCRIPTION GIVEN");
    bool file_id_avail = get_file_idz(&u, dn);
    done = false;

    while (!done && !hangup && !file_id_avail)
    {
        bool abort = false;

        ClearScreen();
        nl();
        sess->bout << "|#1Upload going to |#7" << d.name << "\r\n\n";
		sess->bout << "   |#1Filename    |01: |#7" << pszFileName << wwiv::endl;
		sess->bout << "|#2A|#7] |#1Description |01: |#7" << u.description << wwiv::endl;
        sess->bout << "|#2B|#7] |#1Modify extended description\r\n\n";
        print_extended(u.filename, &abort, 10, 0);
        sess->bout << "|#2<|#7CR|#2> |#1to continue, |#7Q|#1 to abort upload: ";
        key = onek( "\rQABC", true );
        switch (key)
        {
        case 'Q':
            sess->bout << "Are you sure, file will be lost? ";
            if (yesno())
            {
                t2u_error(pszFileName, "Changed mind");
				// move file back to batch dir
                movefile( s2, s1, true );
                return 1;
            }
            break;

        case 'A':
            nl();
			sess->bout << "Please enter a one line description.\r\n:";
            inputl(u.description, 58);
            break;

        case 'B':
            nl();
            ss = read_extended_description(u.filename);
            sess->bout << "|#5Modify extended description? ";
            if (yesno())
            {
                nl();
                if (ss)
                {
                    sess->bout << "|#5Delete it? ";
                    if (yesno())
                    {
                        BbsFreeMemory(ss);
                        delete_extended_description(u.filename);
                        u.mask &= ~mask_extended;
                    }
                    else
                    {
                        u.mask |= mask_extended;
                        modify_extended_description( &ss, directories[udir[sess->GetCurrentFileArea()].subnum].name, u.filename );
                        if (ss)
                        {
                            delete_extended_description(u.filename);
                            add_extended_description(u.filename, ss);
                            BbsFreeMemory(ss);
                        }
                    }
                }
                else
                {
                    modify_extended_description(&ss, directories[udir[sess->GetCurrentFileArea()].subnum].name, u.filename);
                    if (ss)
                    {
                        add_extended_description(u.filename, ss);
                        BbsFreeMemory(ss);
                        u.mask |= mask_extended;
                    }
                    else
                    {
                        u.mask &= ~mask_extended;
                    }
                }
            }
            else if (ss)
            {
                BbsFreeMemory(ss);
                u.mask |= mask_extended;
            }
            else
            {
                u.mask &= ~mask_extended;
            }
            break;

        case '\r':
            nl();
            done = true;
        }
    }

    nl( 3 );

	WFile file( d.path, s );
	if ( !file.Open( WFile::modeBinary|WFile::modeReadOnly ) )
    {
        // dos error, file not found
        if (u.mask & mask_extended)
        {
            delete_extended_description(u.filename);
        }
        t2u_error(pszFileName, "DOS error - File not found.");
        return 1;
    }
    if (syscfg.upload_c[0])
    {
		file.Close();
        sess->bout << "Please wait...\r\n";
        if ( !check_ul_event( dn, &u ) )
        {
            if (u.mask & mask_extended)
			{
                delete_extended_description(u.filename);
			}
            t2u_error(pszFileName, "Failed upload event");
            return 1;
        }
        else
        {
			file.Open( WFile::modeBinary | WFile::modeReadOnly );
        }
    }
	long l = file.GetLength();
    u.numbytes = l;
	file.Close();
    sess->thisuser.SetFilesUploaded( sess->thisuser.GetFilesUploaded() + 1 );

    time(&l);
    u.daten = l;
	WFile fileDownload( g_szDownloadFileName );
	fileDownload.Open( WFile::modeBinary|WFile::modeCreateFile|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite );
    for (i = sess->numf; i >= 1; i--)
    {
        FileAreaSetRecord( fileDownload, i );
		fileDownload.Read( &u1, sizeof( uploadsrec ) );
        FileAreaSetRecord( fileDownload, i + 1 );
		fileDownload.Write( &u1, sizeof( uploadsrec ) );
    }

    FileAreaSetRecord( fileDownload, 1 );
	fileDownload.Write( &u, sizeof( uploadsrec ) );
    ++sess->numf;
    FileAreaSetRecord( fileDownload, 0 );
	fileDownload.Read( &u1, sizeof( uploadsrec ) );
    u1.numbytes = sess->numf;
    u1.daten = l;
    sess->m_DirectoryDateCache[dn] = l;
    FileAreaSetRecord( fileDownload, 0 );
	fileDownload.Write( &u1, sizeof( uploadsrec ) );
	fileDownload.Close();

    modify_database(u.filename, true);

    sess->thisuser.SetUploadK( sess->thisuser.GetUploadK() + bytes_to_k( u.numbytes ) );

    GetApplication()->GetStatusManager()->Lock();
    ++status.uptoday;
    ++status.filechange[filechange_upload];
    GetApplication()->GetStatusManager()->Write();
    sysoplogf( "+ \"%s\" uploaded on %s", u.filename, directories[dn].name);
    return 0;                                 // This means success
}


void t2u_error(char *pszFileName, char *msg)
{
    char szBuffer[ 255 ];

    nl( 2 );
    sprintf(szBuffer, "**  %s failed T2U qualifications", pszFileName);
    sess->bout << szBuffer;
	nl();
    sysoplog(szBuffer);

    sprintf(szBuffer, "** Reason : %s", msg);
    sess->bout << szBuffer;
	nl();
    sysoplog(szBuffer);
}

