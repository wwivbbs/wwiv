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


//
// Local function prototypes
//

void SendLocalShortMessage( unsigned int nUserNum, unsigned int nSystemNum, char *pszMessageText );
void SendRemoteShortMessage( unsigned int nUserNum, unsigned int nSystemNum, char *pszMessageText );


/*
 * Handles reading short messages. This is also where PackScan file requests
 * plug in, if such are used.
 */
void rsm( int nUserNum, WUser *pUser, bool bAskToSaveMsgs )
{
    bool bShownAnyMessage = false;
    int bShownAllMessages = true;
    if ( pUser->HasShortMessage() )
    {
        WFile file( syscfg.datadir, SMW_DAT );
        if ( !file.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile,
                         WFile::shareUnknown, WFile::permReadWrite ) )
        {
            return;
        }
        int nTotalMsgsInFile = static_cast<int>( file.GetLength() / sizeof( shortmsgrec ) );
        for (int nCurrentMsg = 0; nCurrentMsg < nTotalMsgsInFile; nCurrentMsg++)
        {
            shortmsgrec sm;
            file.Seek( nCurrentMsg * sizeof( shortmsgrec ), WFile::seekBegin );
            file.Read( &sm, sizeof( shortmsgrec ) );
            if ( sm.touser == nUserNum && sm.tosys == 0 )
            {
                GetSession()->bout.Color( 9 );
				GetSession()->bout << sm.message;
				GetSession()->bout.NewLine();
                bool bHandledMessage = false;
                bShownAnyMessage = true;
                if ( !so() || !bAskToSaveMsgs )
                {
                    bHandledMessage = true;
                }
                else
                {
                    if ( GetApplication()->HasConfigFlag( OP_FLAGS_CAN_SAVE_SSM ) )
                    {
                        if ( !bHandledMessage && bAskToSaveMsgs )
                        {
                            GetSession()->bout << "|#5Would you like to save this notification? ";
                            bHandledMessage = !yesno();
                        }
                    }
                    else
                    {
                        bHandledMessage = true;
                    }

                }
                if ( bHandledMessage )
                {
                    sm.touser = 0;
                    sm.tosys = 0;
                    sm.message[0] = 0;
                    file.Seek( nCurrentMsg * sizeof( shortmsgrec ), WFile::seekBegin );
                    file.Write( &sm, sizeof( shortmsgrec ) );
                }
                else
                {
                    bShownAllMessages = false;
                }
            }
        }
        file.Close();
        smwcheck = true;
    }
    if ( bShownAnyMessage )
    {
        GetSession()->bout.NewLine();
    }
    if ( bShownAllMessages )
    {
        pUser->SetStatusFlag( WUser::SMW );
    }
}


void SendLocalShortMessage( unsigned int nUserNum, unsigned int nSystemNum, char *pszMessageText )
{
    WUser user;
    GetApplication()->GetUserManager()->ReadUser( &user, nUserNum );
    if ( !user.IsUserDeleted() )
    {
        WFile file( syscfg.datadir, SMW_DAT );
        if ( !file.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile,
                         WFile::shareUnknown, WFile::permReadWrite ) )
        {
            return;
        }
        int nTotalMsgsInFile = static_cast<int>(file.GetLength() / sizeof(shortmsgrec));
        int nNewMsgPos = nTotalMsgsInFile - 1;
        shortmsgrec sm;
        if (nNewMsgPos >= 0)
        {
            file.Seek( nNewMsgPos * sizeof( shortmsgrec ), WFile::seekBegin );
            file.Read( &sm, sizeof( shortmsgrec ) );
            while ( sm.tosys == 0 && sm.touser == 0 && nNewMsgPos > 0 )
            {
                --nNewMsgPos;
                file.Seek( nNewMsgPos * sizeof( shortmsgrec ), WFile::seekBegin );
                file.Read( &sm, sizeof( shortmsgrec ) );
            }
            if ( sm.tosys != 0 || sm.touser != 0 )
            {
                nNewMsgPos++;
            }
        }
        else
        {
            nNewMsgPos = 0;
        }
        sm.tosys = static_cast<unsigned short>( nSystemNum );
        sm.touser = static_cast<unsigned short>( nUserNum );
        strncpy( sm.message, pszMessageText, 80 );
        sm.message[80] = '\0';
        file.Seek( nNewMsgPos * sizeof( shortmsgrec ), WFile::seekBegin );
        file.Write( &sm, sizeof( shortmsgrec ) );
        file.Close();
        user.SetStatusFlag( WUser::SMW );
        GetApplication()->GetUserManager()->WriteUser( &user, nUserNum );
    }
}

void SendRemoteShortMessage( int nUserNum, int nSystemNum, char *pszMessageText )
{
    net_header_rec nh;
    nh.tosys = static_cast<unsigned short>( nSystemNum );
    nh.touser = static_cast<unsigned short>( nUserNum );
    nh.fromsys = net_sysnum;
    nh.fromuser = static_cast<unsigned short>( GetSession()->usernum );
    nh.main_type = main_type_ssm;
    nh.minor_type = 0;
    nh.list_len = 0;
    nh.daten = static_cast<unsigned long>(time(NULL));
    if (strlen(pszMessageText) > 80)
    {
        pszMessageText[80] = '\0';
    }
    nh.length = strlen(pszMessageText);
    nh.method = 0;
    char szPacketName[MAX_PATH];
    sprintf( szPacketName, "%sp0%s", GetSession()->GetNetworkDataDirectory(), GetApplication()->GetNetworkExtension() );
    WFile file( szPacketName );
    file.Open( WFile::modeReadWrite|WFile::modeBinary|WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite );
    file.Seek( 0L, WFile::seekBegin );
    file.Write( &nh, sizeof( net_header_rec ) );
    file.Write( pszMessageText, nh.length );
    file.Close();
}



/*
 * Sends a short message to a user. Takes user number and system number
 * and short message as params. Network number should be set before calling
 * this function.
 */
void ssm( int nUserNum, int nSystemNum, const char *pszFormat, ... )
{
    if ( nUserNum == 65535 || nUserNum == 0 || nSystemNum == 32767 )
    {
        return;
    }

    va_list ap;
    char szMessageText[2048];

    va_start(ap, pszFormat);
    WWIV_VSNPRINTF(szMessageText, sizeof( szMessageText ), pszFormat, ap);
    va_end(ap);

    if ( nSystemNum == 0 )
    {
        SendLocalShortMessage( nUserNum, nSystemNum, szMessageText );
    }
    else if ( net_sysnum && valid_system( nSystemNum ) )
    {
        SendRemoteShortMessage( nUserNum, nSystemNum, szMessageText );
    }
}


