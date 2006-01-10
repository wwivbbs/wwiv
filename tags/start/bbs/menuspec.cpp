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

/* ---------------------------------------------------------------------- */
/* MENUSPEC.C - Menu Specific support functions                           */
/*                                                                        */
/* Functions that dont have any direct WWIV function go in here           */
/* ie, functions to help emulate other BBS's.                             */
/* ---------------------------------------------------------------------- */


/**
 *  Download a file
 *
 *  pszDirFileName      = fname of your directory record
 *  pszDownloadFileName = Filename to download
 *  bFreeDL             = true if this is a free download
 *  bTitle              = true if title is to be shown with file info
 */
int MenuDownload( char *pszDirFileName, char *pszDownloadFileName, bool bFreeDL, bool bTitle )
{
    int bOkToDL;
    uploadsrec u;
    WUser ur;
    char s1[81], s2[81];
    bool abort = false, next = false;

    int dn = FindDN( pszDirFileName );

    if ( dn == -1 )
    {
        MenuSysopLog("DLDNF");                  /* DL - DIR NOT FOUND */
        return 0;
    }
    dliscan1(dn);
    int nRecordNumber = recno( pszDownloadFileName );
    if (nRecordNumber <= 0)
    {
        next = 0;
        checka(&abort, &next);
        if (abort)
        {
            return -1;
        }
        else
        {
            MenuSysopLog("DLFNF");                /* DL - FILE NOT FOUND */
            return 0;
        }
    }
    bool ok = true;
    while ( ( nRecordNumber > 0 ) && ok && !hangup )
    {
        app->localIO->tleft( true );
		WFile fileDownload( g_szDownloadFileName );
		fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly );
        FileAreaSetRecord( fileDownload, nRecordNumber );
		fileDownload.Read( &u, sizeof( uploadsrec ) );
		fileDownload.Close();
        nl();

        if (bTitle)
        {
			sess->bout << "Directory  : " << directories[dn].name << wwiv::endl;
        }
        bOkToDL = printfileinfo(&u, dn);


        if ( strncmp(u.filename, "WWIV4", 5) == 0 && !app->HasConfigFlag( OP_FLAGS_NO_EASY_DL ) )
        {
            bOkToDL = 1;
        }
        else
        {
            if (!ratio_ok())
            {
                return -1;
            }
        }
        if ( bOkToDL || bFreeDL )
        {
            write_inst( INST_LOC_DOWNLOAD, udir[sess->GetCurrentFileArea()].subnum, INST_FLAGS_NONE );
            sprintf( s1, "%s%s", directories[dn].path, u.filename );
            if (directories[dn].mask & mask_cdrom)
            {
                sprintf(s2, "%s%s", directories[dn].path, u.filename);
                sprintf(s1, "%s%s", syscfgovr.tempdir, u.filename);
                if (!WFile::Exists(s1))
                {
                    copyfile(s2, s1, false);
                }
            }
            bool sent = false;
            if (bOkToDL == -1)
            {
                send_file(s1, &sent, &abort, u.filetype, u.filename, dn, -2L);
            }
            else
            {
                send_file(s1, &sent, &abort, u.filetype, u.filename, dn, u.numbytes);
            }

            if (sent)
            {
                if (!bFreeDL)
                {
                    sess->thisuser.SetFilesDownloaded( sess->thisuser.GetFilesDownloaded() + 1 );
                    sess->thisuser.SetDownloadK( sess->thisuser.GetDownloadK() + static_cast<int>( bytes_to_k( u.numbytes ) ) );
                }
                ++u.numdloads;
				fileDownload.Open( WFile::modeBinary|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite );
                FileAreaSetRecord( fileDownload, nRecordNumber );
				fileDownload.Write( &u, sizeof( uploadsrec ) );
				fileDownload.Close();

                sysoplogf( "Downloaded \"%s\"", u.filename);

                if (syscfg.sysconfig & sysconfig_log_dl)
                {
                    app->userManager->ReadUser( &ur, u.ownerusr );
                    if ( !ur.isUserDeleted() )
                    {
                        if ( date_to_daten( ur.GetFirstOn() ) < static_cast<time_t>( u.daten ) )
                        {
                            ssm( u.ownerusr, 0, "%s downloaded '%s' on %s",
                                 sess->thisuser.GetUserNameAndNumber( sess->usernum ),
                                 u.filename, date() );
                        }
                    }
                }
            }

            nl( 2 );
            bprintf("Your ratio is now: %-6.3f\r\n", ratio());

            if ( sess->IsUserOnline() )
            {
                app->localIO->UpdateTopScreen();
            }
        }
        else
        {
            sess->bout << "\r\n\nNot enough time left to D/L.\r\n";
        }
        if (abort)
        {
            ok = false;
        }
        else
        {
            nRecordNumber = nrecno( pszDownloadFileName, nRecordNumber );
        }
    }
    return ( abort ) ? -1 : 1;
}


int FindDN( char *pszDownloadFileName )
{
    for (int i = 0; (i < sess->num_dirs); i++)
    {
        if ( wwiv::stringUtils::IsEqualsIgnoreCase( directories[i].filename, pszDownloadFileName ) )
        {
            return i;
        }
    }
    return -1;
}


/**
 * Run a Door (chain)
 *
 * pszDoor = Door description to run
 * bFree  = If true, security on door will not back checked
 */
bool MenuRunDoorName(char *pszDoor, int bFree)
{
    int nDoorNumber = FindDoorNo( pszDoor );
    if (nDoorNumber < 0)
    {
        return false;
    }
    return MenuRunDoorNumber( nDoorNumber, bFree );
}


bool MenuRunDoorNumber(int nDoorNumber, int bFree)
{
    if ( !bFree && !ValidateDoorAccess( nDoorNumber ) )
    {
        return false;
    }

    run_chain( nDoorNumber );
    return true;
}



int FindDoorNo( char *pszDoor )
{
    for ( int i = 0; i < sess->GetNumberOfChains(); i++ )
    {
        if ( wwiv::stringUtils::IsEqualsIgnoreCase( chains[i].description, pszDoor ) )
        {
            return i;
        }
    }

    return -1;
}


bool ValidateDoorAccess( int nDoorNumber )
{
    int inst = (inst_ok(INST_LOC_CHAINS, nDoorNumber + 1));
    if (inst != 0)
    {
        char szChainInUse[255];
		sprintf( szChainInUse,  "|#2Chain %s is in use on instance %d.  ", chains[nDoorNumber].description, inst );
        if (!(chains[nDoorNumber].ansir & ansir_multi_user))
        {
            sess->bout << szChainInUse << " Try again later.\r\n";
            return false;
        }
        else
        {
            sess->bout << szChainInUse << " Care to join in? ";
            if (!(yesno()))
            {
                return false;
            }
        }
    }
    chainfilerec c = chains[nDoorNumber];
    if ((c.ansir & ansir_ansi) && ( !okansi() ))
    {
        return false;
    }
    if ((c.ansir & ansir_local_only) && (sess->using_modem))
    {
        return false;
    }
    if (c.sl > sess->GetEffectiveSl())
    {
        return false;
    }
    if ( c.ar && !sess->thisuser.hasArFlag( c.ar ) )
    {
        return false;
    }
    if ( app->HasConfigFlag( OP_FLAGS_CHAIN_REG ) && chains_reg && sess->GetEffectiveSl() < 255 )
    {
        chainregrec r = chains_reg[ nDoorNumber ];
        if ( r.maxage )
        {
            if ( r.minage > sess->thisuser.GetAge() || r.maxage < sess->thisuser.GetAge() )
            {
                return false;
            }
        }
    }
    // passed all the checks, return true
    return true;
}


/* ----------------------- */
/* End of run door section */
/* ----------------------- */
void ChangeSubNumber()
{
    sess->bout << "|#7Select Sub number : |#0";

    char* s = mmkey( 0 );
    for (int i = 0; (i < sess->num_subs) && (usub[i].subnum != -1); i++)
    {
        if ( wwiv::stringUtils::IsEquals( usub[i].keys, s ) )
        {
            sess->SetCurrentMessageArea( i );
        }
    }
}


void ChangeDirNumber()
{
    bool done = false;
    while (!done && !hangup)
    {
        sess->bout << "|#7Select Dir number : |#0";

        char* s = mmkey( 1 );

        if (s[0] == '?')
        {
            DirList();
            nl();
            continue;
        }
        for (int i = 0; i < sess->num_dirs; i++)
        {
            if ( wwiv::stringUtils::IsEquals( udir[i].keys, s ) )
            {
                sess->SetCurrentFileArea( i );
                done = true;
            }
        }
    }
}


// have a little conference ability...
void SetMsgConf(int iConf)
{
    int nc;
    confrec *cp;
    userconfrec *uc;

    get_conf_info(CONF_SUBS, &nc, &cp, NULL, NULL, &uc);

    for (int i = 0; (i < MAX_CONFERENCES) && (uc[i].confnum != -1); i++)
    {
        if (iConf == cp[uc[i].confnum].designator)
        {
            setuconf(CONF_SUBS, i, -1);
            break;
        }
    }
}


void SetDirConf(int iConf)
{
    int nc;
    confrec *cp;
    userconfrec *uc;

    get_conf_info(CONF_DIRS, &nc, &cp, NULL, NULL, &uc);

    for (int i = 0; (i < MAX_CONFERENCES) && (uc[i].confnum != -1); i++)
    {
        if (iConf == cp[uc[i].confnum].designator)
        {
            setuconf(CONF_DIRS, i, -1);
            break;
        }
    }
}


void EnableConf()
{
    tmp_disable_conf( false );
}


void DisableConf()
{
    tmp_disable_conf( true );
}


void SetNewScanMsg()
{
    sysoplog("Select Subs");
    config_qscan();
}


void ReadMessages()
{
    char szWhere[15];

    sess->bout << "\r\n|#8Which messages?\r\n|#7(N)ew (A)ll (Q)uit : ";
    int iWhich = onek("QNA");
    if (iWhich == 'Q')
    {
        return;
    }

    while (!hangup)
    {
        sess->bout << "|#8Which Subs?\r\n";
        sess->bout << "|#7(S)elected  (A)ll Subs  (L)ist  (Q)uit or Sub# : |#0";

        input(szWhere, 3);

        if (szWhere[0] == 'Q')
        {
            return;
        }


        if (szWhere[0] == '?' || szWhere[0] == 'L')
        {
            SubList();
        }

        if (szWhere[0] == 'S')
        {
            if (iWhich == 'N')
            {
                // Read new messages in the qscan subs
                ReadSelectedMessages(RM_QSCAN_MSGS, RM_QSCAN_SUBS);
                return;
            }
            if (iWhich == 'A')
            {
                // Read all messages in the qscan subs
                ReadSelectedMessages(RM_ALL_MSGS, RM_QSCAN_SUBS);
            }
            break;
        }
        if (szWhere[0] == 'A')
        {
            if (iWhich == 'N')
            {
                // Read new messages in all of the subs
                ReadSelectedMessages(RM_QSCAN_MSGS, RM_ALL_SUBS);
            }
            if (iWhich == 'A')
            {
                // Read new messages in all of the subs
                ReadSelectedMessages(RM_ALL_MSGS, RM_ALL_SUBS);
            }
            break;
        }
        if (isdigit(szWhere[0]))
        {
            for (int iTemp = 0; (iTemp < sess->num_subs) && (usub[iTemp].subnum != -1); iTemp++)
            {
                if ( wwiv::stringUtils::IsEquals( usub[iTemp].keys, szWhere ) )
                {
                    sess->SetCurrentMessageArea( iTemp );

                    if ( iWhich == 'N' )
                    {
                        // read new messages in the current sub
                        ReadSelectedMessages( RM_QSCAN_MSGS, sess->GetCurrentMessageArea() );
                    }
                    if ( iWhich == 'A' )
                    {
                        // Read all messages in the current sub
                        ReadSelectedMessages( RM_ALL_MSGS, sess->GetCurrentMessageArea() );
                    }
                }
            }
            break;
        }
    }
}


void ReadSelectedMessages(int iWhich, int iWhere)
{
    int iSaveSub = sess->GetCurrentMessageArea();
    int i, nextsub;
    bool abort, next;

    switch (iWhere)
    {
    case RM_ALL_SUBS:
    case RM_QSCAN_SUBS:
        ClearScreen();

        nextsub = 1;

		sess->bout << "|#3-=< Q-Scan All >=-\r\n";
        for (i = 0; (usub[i].subnum != -1) && (i < sess->num_subs) && (nextsub) && (!hangup); i++)
        {
            sess->SetCurrentMessageArea( i );
            iscan( sess->GetCurrentMessageArea() );
            if (iWhere == RM_QSCAN_SUBS)
            {
                if (qsc_q[usub[i].subnum / 32] & (1L << (usub[i].subnum % 32)))
                {
                    if (iWhich == RM_QSCAN_MSGS)
                    {
                        qscan(i, &nextsub);
                    }
                    else if (iWhich == RM_ALL_MSGS)
                    {
						scan(1, SCAN_OPTION_READ_MESSAGE, &nextsub, false );
                    }
                }
            }
            else
            {
                if (iWhich == RM_QSCAN_MSGS)
                {
                    qscan(i, &nextsub);
                }
                else if (iWhich == RM_ALL_MSGS)
                {
                    scan(1, SCAN_OPTION_READ_MESSAGE, &nextsub, false );
                }
            }

            abort = next = false;
            checka(&abort, &next);
            if (abort)
                nextsub = 0;
        }
        nl();
        sess->bout << "|#3-=< Global Q-Scan Done >=-\r\n\n";
        break;

    default:                                /* sub # */
        sess->SetCurrentMessageArea( iWhere );

        if ( iWhich == RM_QSCAN_MSGS )
        {
            qscan( iWhere, &nextsub );
        }
        else if ( iWhich == RM_ALL_MSGS )
        {
            scan( 1, SCAN_OPTION_READ_MESSAGE, &nextsub, false );
        }
        break;
    }

    sess->SetCurrentMessageArea( iSaveSub );
    iscan( sess->GetCurrentMessageArea() );
}
