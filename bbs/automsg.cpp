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


char ShowAMsgMenuAndGetInput( const char *pszAutoMessageLockFileName );
void write_automessage1();


/**
 * Reads the auto message
 */
void read_automessage()
{
    char l[6][81];

    WFile file( syscfg.gfilesdir, AUTO_MSG );
    GetSession()->bout.NewLine();
    WStatus* pStatus = GetApplication()->GetStatusManager()->GetStatus(); 
    bool bAutoMessageAnonymous = pStatus->IsAutoMessageAnonymous();
    delete pStatus;

    if ( !file.Open( WFile::modeReadOnly | WFile::modeBinary ) )
    {
        GetSession()->bout << "|13No auto-message.\r\n";
    }
    else
    {
        char szAutoMsgBuffer[512];
        long lAutoMsgFileLen = file.Read( szAutoMsgBuffer, 512 );
        file.Close();

        int ptrbeg[10], ptrend[10];
        for (int i = 0; i < 10; i++)
        {
            ptrbeg[i] = '\0';
            ptrend[i] = '\0';
        }

        bool bNeedLF = false;
        int nCurrentLineNum = 0;
        for ( int nFilePos = 0; nFilePos < lAutoMsgFileLen; nFilePos++ )
        {
            if ( bNeedLF )
            {
                if ( szAutoMsgBuffer[ nFilePos ] == SOFTRETURN )
                {
                    ptrbeg[ nCurrentLineNum ] = nFilePos + 1;
                    bNeedLF = false;
                }
            }
            else
            {
                if ( szAutoMsgBuffer[ nFilePos ] == RETURN )
                {
                    ptrend[nCurrentLineNum] = nFilePos - 1;
                    if (nCurrentLineNum < 6)
                    {
                        for (int i3 = ptrbeg[nCurrentLineNum]; i3 <= ptrend[nCurrentLineNum]; i3++)
                        {
                            l[nCurrentLineNum][i3 - ptrbeg[nCurrentLineNum]] = szAutoMsgBuffer[i3];
                        }
                        l[nCurrentLineNum][ptrend[nCurrentLineNum] - ptrbeg[nCurrentLineNum] + 1] = 0;
                    }
                    ++nCurrentLineNum;
                    bNeedLF = true;
                }
            }
        }
        char szAuthorName[ 81 ];
        if ( bAutoMessageAnonymous )
        {
            if ( getslrec( GetSession()->GetEffectiveSl() ).ability & ability_read_post_anony )
            {
                sprintf(szAuthorName, "<<< %s >>>", &(l[0][0]));
            }
            else
            {
                strcpy( szAuthorName, ">UNKNOWN<" );
            }
        }
        else
        {
            strcpy( szAuthorName, &(l[ 0 ][ 0 ]) );
        }
        GetSession()->bout << "\r\n|#9Auto message by: |#2" << szAuthorName << "|#0\r\n\n";
        int nLineNum = 1;
        while ( ptrend[ nLineNum ] && nLineNum < 6 )
        {
            GetSession()->bout.Color( 9 );
            GetSession()->bout << &( l[ nLineNum ][ 0 ] );
			GetSession()->bout.NewLine();
            ++nLineNum;
        }
    }
    GetSession()->bout.NewLine();
}


/**
 * Writes the auto message
 */
void write_automessage1()
{
    char l[ 6 ][ 81 ], szRollOverLine[ 81 ];

    szRollOverLine[ 0 ] = '\0';

    GetSession()->bout << "\r\n|#9Enter auto-message. Max 5 lines. Colors allowed:|#0\r\n\n";
    for (int i = 0; i < 5; i++)
    {
        GetSession()->bout << "|#7" << i + 1 << ":|#0";
        inli( &(l[i][0]), szRollOverLine, 70, true, false );
        strcat( &(l[i][0]), "\r\n" );
    }
    GetSession()->bout.NewLine();
    bool bAnonStatus = false;
    if ( getslrec( GetSession()->GetEffectiveSl() ).ability & ability_post_anony )
    {
        GetSession()->bout << "|#9Anonymous? ";
        bAnonStatus = yesno();
    }

    GetSession()->bout << "|#9Is this OK? ";
    if ( yesno() )
    {
        WStatus *pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
        pStatus->SetAutoMessageAnonymous( bAnonStatus );
        pStatus->SetAutoMessageAuthorUserNumber( GetSession()->usernum );
        GetApplication()->GetStatusManager()->CommitTransaction( pStatus );
        WFile file( syscfg.gfilesdir, AUTO_MSG );
        file.Open( WFile::modeReadWrite | WFile::modeCreateFile | WFile::modeBinary | WFile::modeTruncate, WFile::shareUnknown, WFile::permReadWrite );
        char szAuthorName[ 81 ];
		sprintf( szAuthorName, "%s\r\n", GetSession()->GetCurrentUser()->GetUserNameAndNumber( GetSession()->usernum ) );
        file.Write( szAuthorName, strlen( szAuthorName ) );
        for (int j = 0; j < 5; j++)
        {
            file.Write( &(l[j][0]), strlen(&(l[j][0])));
        }
        sysoplog("Changed Auto-message");
        for (int k = 0; k < 5; k++)
        {
            char szLogLine[ 255 ];
            strcpy(szLogLine, "  ");
            l[k][strlen(&(l[k][0])) - 2] = 0;
            strcat(szLogLine, &(l[k][0]));
            sysoplog(szLogLine);
        }
        GetSession()->bout << "\r\n|10Auto-message saved.\r\n\n";
        file.Close();
    }
}


char ShowAMsgMenuAndGetInput( const char *pszAutoMessageLockFileName )
{
    bool bCanWrite = false;
    if ( !GetSession()->GetCurrentUser()->IsRestrictionAutomessage() && !WFile::Exists( pszAutoMessageLockFileName ) )
    {
        bCanWrite = ( getslrec( GetSession()->GetEffectiveSl() ).posts ) ? true : false;
    }

    char cmdKey = 0;
    if (cs())
    {
        GetSession()->bout << "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply, (|#2W|#9)rite, (|#2L|#9)ock, (|#2D|#9)el, (|#2U|#9)nlock : ";
        cmdKey = onek( "QRWALDU", true );
    }
    else if (bCanWrite)
    {
        GetSession()->bout << "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply, (|#2W|#9)rite : ";
        cmdKey = onek( "QRWA", true );
    }
    else
    {
        GetSession()->bout << "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply : ";
        cmdKey = onek( "QRA", true );
    }
    return cmdKey;
}

/**
 * Main Automessage menu.  Displays the auto message then queries for input.
 */
void do_automessage()
{
    char aMsgLockFile[ MAX_PATH ], aMsgFile[ MAX_PATH ];
    sprintf(aMsgLockFile, "%s%s", syscfg.gfilesdir, LOCKAUTO_MSG);
    sprintf(aMsgFile, "%s%s", syscfg.gfilesdir, AUTO_MSG);

    // initally show the auto message
    read_automessage();

    bool done = false;
    do
    {
        GetSession()->bout.NewLine();
        char cmdKey = ShowAMsgMenuAndGetInput( aMsgLockFile );
        switch (cmdKey)
        {
        case 'Q':
            done = true;
            break;
        case 'R':
            read_automessage();
            break;
        case 'W':
            write_automessage1();
            break;
        case 'A':
            {
                grab_quotes(NULL, NULL);
                WStatus *pStatus = GetApplication()->GetStatusManager()->GetStatus();
                if (pStatus->GetAutoMessageAuthorUserNumber() > 0)
                {
                    strcpy(irt, "Re: AutoMessage");
                    email( pStatus->GetAutoMessageAuthorUserNumber(), 0, false, pStatus->IsAutoMessageAnonymous() ? anony_sender : 0 );
                }
                delete pStatus;
            }
            break;
        case 'D':
            GetSession()->bout << "\r\n|13Delete Auto-message, Are you sure? ";
            if (yesno())
            {
                WFile::Remove(aMsgFile);
            }
            GetSession()->bout.NewLine( 2 );
            break;
        case 'L':
            if (WFile::Exists(aMsgLockFile))
            {
                GetSession()->bout << "\r\n|13Message is already locked.\r\n\n";
            }
            else
            {
                GetSession()->bout <<  "|#9Do you want to lock the Auto-message? ";
                if ( yesno() )
                {
                    /////////////////////////////////////////////////////////
                    // This makes a file in your GFILES dir 1 bytes long,
                    // to tell the board if it is locked or not. It consists
                    // of a space.
                    //
                    FILE* lock_auto = fopen(aMsgLockFile, "w+t");
                    fputc(' ', lock_auto);
                    fclose(lock_auto);
                }
            }
            break;
        case 'U':
            if (!WFile::Exists(aMsgLockFile))
            {
                GetSession()->bout << "Message not locked.\r\n";
            }
            else
            {
                GetSession()->bout << "|#5Unlock message? ";
                if (yesno())
                {
                    WFile::Remove(aMsgLockFile);
                }
            }
            break;
        }
    } while ( !done && !hangup );

}
