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
#include <sstream>

//////////////////////////////////////////////////////////////////////////////
//
// Implementation
//
//
//

/**
 * Displays a horizontal bar of nSize characters in nColor
 * @param nSize Length of the horizontal bar to display
 * @param nColor Color of the horizontal bar to display
 */
void DisplayHorizontalBar( int nSize, int nColor )
{
    unsigned char ch = ( okansi() ) ? '\xC4' : '-';
    repeat_char( ch, nSize, nColor, true );
}


/**
 * Displays some basic user statistics for the current user.
 */
void YourInfo()
{
    GetSession()->bout.ClearScreen();
    if ( okansi() )
    {
        GetSession()->bout.DisplayLiteBar( "[ Your User Information ]" );
    }
    else
    {
        GetSession()->bout << "|10Your User Information:\r\n";
    }
    GetSession()->bout.NewLine();
	GetSession()->bout << "|#9Your name      : |#2" << GetSession()->GetCurrentUser()->GetUserNameAndNumber( GetSession()->usernum ) << wwiv::endl;
	GetSession()->bout << "|#9Phone number   : |#2" << GetSession()->GetCurrentUser()->GetVoicePhoneNumber() << wwiv::endl;
    if ( GetSession()->GetCurrentUser()->GetNumMailWaiting() > 0 )
    {
		GetSession()->bout << "|#9Mail Waiting   : |#2" << GetSession()->GetCurrentUser()->GetNumMailWaiting() << wwiv::endl;
    }
	GetSession()->bout << "|#9Security Level : |#2" << GetSession()->GetCurrentUser()->GetSl() << wwiv::endl;
    if ( GetSession()->GetEffectiveSl() != GetSession()->GetCurrentUser()->GetSl() )
    {
		GetSession()->bout << "|#1 (temporarily |#2" << GetSession()->GetEffectiveSl() << "|#1)";
    }
    GetSession()->bout.NewLine();
	GetSession()->bout << "|#9Transfer SL    : |#2" << GetSession()->GetCurrentUser()->GetDsl() << wwiv::endl;
    GetSession()->bout << "|#9Date Last On   : |#2" << GetSession()->GetCurrentUser()->GetLastOn() << wwiv::endl;
    GetSession()->bout << "|#9Times on       : |#2" << GetSession()->GetCurrentUser()->GetNumLogons() << wwiv::endl;
    GetSession()->bout << "|#9On today       : |#2" << GetSession()->GetCurrentUser()->GetTimesOnToday() << wwiv::endl;
    GetSession()->bout << "|#9Messages posted: |#2" << GetSession()->GetCurrentUser()->GetNumMessagesPosted() << wwiv::endl;
    GetSession()->bout << "|#9E-mail sent    : |#2" << ( GetSession()->GetCurrentUser()->GetNumEmailSent() + GetSession()->GetCurrentUser()->GetNumFeedbackSent() + GetSession()->GetCurrentUser()->GetNumNetEmailSent() );
    GetSession()->bout << "|#9Time spent on  : |#2" << static_cast<long>( ( GetSession()->GetCurrentUser()->GetTimeOn() + timer() - timeon ) / SECONDS_PER_MINUTE_FLOAT ) << " |#9Minutes" << wwiv::endl;

    // Transfer Area Statistics
    GetSession()->bout << "|#9Uploads        : |#2" << GetSession()->GetCurrentUser()->GetUploadK() << "|#9k in|#2 " << GetSession()->GetCurrentUser()->GetFilesUploaded() << " |#9files\r\n";
    GetSession()->bout << "|#9Downloads      : |#2" << GetSession()->GetCurrentUser()->GetDownloadK()<< "|#9k in|#2 " << GetSession()->GetCurrentUser()->GetFilesDownloaded() << " |#9files\r\n";
	GetSession()->bout << "|#9Transfer Ratio : |#2" << ratio() << wwiv::endl;
    GetSession()->bout.NewLine();
    pausescr();
}


/**
 * Gets the maximum number of lines allowed for a post by the current user.
 * @return The maximum message length in lines
 */
int GetMaxMessageLinesAllowed()
{
	if ( so() )
	{
		return 120;
	}
	else if ( cs() )
	{
		return 100;
	}
    return 80;
}


/**
 * Allows user to upload a post.
 */
void upload_post()
{
    WFile file( syscfgovr.tempdir, INPUT_MSG );
    long lMaxBytes = 250 * static_cast<long>( GetMaxMessageLinesAllowed() );

	GetSession()->bout << "\r\nYou may now upload a message, max bytes: " << lMaxBytes << wwiv::endl << wwiv::endl;
    char ch = '\0';
    int i = 0;
    receive_file( file.GetFullPathName(), &i, &ch, INPUT_MSG, -1 );
    if ( file.Open( WFile::modeReadOnly | WFile::modeBinary ) )
    {
        long lFileSize = file.GetLength();
        if ( lFileSize > lMaxBytes )
        {
			GetSession()->bout << "\r\n|12Sorry, your message is too long.  Not saved.\r\n\n";
            file.Close();
            file.Delete();
        }
        else
        {
            file.Close();
            use_workspace = true;
            GetSession()->bout << "\r\n|#7* |#1Message uploaded.  The next post or email will contain that text.\r\n\n";
        }
    }
    else
    {
        GetSession()->bout << "\r\n|13Nothing saved.\r\n\n";
    }
}


/**
 * High-level function for sending email.
 */
void send_email()
{
    char szUserName[81];

    write_inst(INST_LOC_EMAIL, 0, INST_FLAGS_NONE);
	GetSession()->bout << "\r\n\n|#9Enter user name or number:\r\n:";
    input( szUserName, 75, true );
    irt[0] = '\0';
    irt_name[0] = '\0';
    unsigned int i;
    if (((i = strcspn(szUserName, "@")) != (strlen(szUserName))) && (isalpha(szUserName[i + 1])))
    {
        if (strstr(szUserName, "@32767") == NULL)
        {
            WWIV_STRLWR(szUserName);
            strcat(szUserName, " @32767");
        }
    }

    int nSystemNumber, nUserNumber;
    parse_email_info(szUserName, &nUserNumber, &nSystemNumber);
    grab_quotes( NULL, NULL );
    if ( nUserNumber || nSystemNumber )
    {
        email( nUserNumber, nSystemNumber, false, 0 );
    }
}

/**
 * High-level function for selecting conference type to edit.
 */
void edit_confs()
{
    if (!ValidateSysopPassword())
    {
        return;
    }

    while ( !hangup )
    {
        GetSession()->bout << "\r\n\n|10Edit Which Conferences:\r\n\n";
        GetSession()->bout << "|#21|#9)|#1 Subs\r\n";
        GetSession()->bout << "|#22|#9)|#1 Dirs\r\n";
        GetSession()->bout << "\r\n|#9Select [|#21|#9,|#22|#9,|#2Q|#9]: ";
        char ch = onek( "Q12", true );
        switch (ch)
        {
        case '1':
            conf_edit( CONF_SUBS );
            break;
        case '2':
            conf_edit( CONF_DIRS );
            break;
        case 'Q':
            return;
        }
        CheckForHangup();
    }
}

/**
 * Sends Feedback to the SysOp.  If  bNewUserFeedback is true then this is
 * newuser feedback, otherwise it is "normal" feedback.
 * The user can choose to email anyone listed.
 * Users with GetSession()->usernum < 10 who have sysop privs will be listed, so
 * this user can select which sysop to leave feedback to.
 */
void feedback( bool bNewUserFeedback )
{
    int i;
    char onek_str[20], ch;

    irt_name[0] = '\0';
    grab_quotes( NULL, NULL );

    if ( bNewUserFeedback )
    {
        sprintf( irt, "|#1Validation Feedback (|12%d|#2 slots left|#1)", syscfg.maxusers - GetApplication()->GetStatusManager()->GetUserCount() );
        // We disable the fsed here since it was hanging on some systems.  Not sure why
        // but it's better to be safe -- Rushfan 2003-12-04
        email( 1, 0, true, 0, true, false );
        return;
    }
    if ( guest_user )
    {
        GetApplication()->GetStatusManager()->RefreshStatusCache();
        strcpy( irt, "Guest Account Feedback" );
        email( 1, 0, true, 0, true, true );
        return;
    }
    strcpy( irt, "|#1Feedback" );
    int nNumUserRecords = GetApplication()->GetUserManager()->GetNumberOfUserRecords();
    int i1 = 0;

    for ( i = 2; i < 10 && i < nNumUserRecords; i++ )
    {
        WUser user;
        GetApplication()->GetUserManager()->ReadUser( &user, i );
        if ( ( user.GetSl() == 255 || ( getslrec( user.GetSl() ).ability & ability_cosysop ) ) &&
            !user.isUserDeleted() )
        {
            i1++;
        }
    }

    if ( !i1 )
    {
        i = 1;
    }
    else
    {
        onek_str[0] = '\0';
        i1 = 0;
        GetSession()->bout.NewLine();
        for ( i = 1; ( i < 10 && i < nNumUserRecords ); i++ )
        {
            WUser user;
            GetApplication()->GetUserManager()->ReadUser( &user, i );
            if ( ( user.GetSl() == 255 || (getslrec( user.GetSl() ).ability & ability_cosysop ) ) &&
                 !user.isUserDeleted() )
            {
				GetSession()->bout << "|#2" << i << "|#7)|#1 " << user.GetUserNameAndNumber( i ) << wwiv::endl;
                onek_str[i1++] = static_cast< char >( '0' + i );
            }
        }
        onek_str[i1++] = *str_quit;
        onek_str[i1] = '\0';
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#1Feedback to (" << onek_str << "): ";
        ch = onek( onek_str, true );
        if ( ch == *str_quit )
        {
            return;
        }
        GetSession()->bout.NewLine();
        i = ch - '0';
    }
    email( static_cast< unsigned short >( i ), 0, false, 0, true );
}


/**
 * Allows editing of ASCII textfiles. Must have an external editor defined,
 * and toggled for use in defaults.
 */
void text_edit()
{
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#9Enter Filename: ";
    char szFileName[ MAX_PATH ];
    input( szFileName, 12, true );
    if ( strstr( szFileName, ".log" ) != NULL || !okfn( szFileName ) )
	{
        return;
	}
	std::stringstream logText;
    logText << "@ Edited: " << szFileName;
    sysoplog( logText.str().c_str() );
    if ( okfsed() )
	{
		external_edit( szFileName, syscfg.gfilesdir, GetSession()->GetCurrentUser()->GetDefaultEditor() - 1, 500, syscfg.gfilesdir, logText.str().c_str(), MSGED_FLAG_NO_TAGLINE );
	}
}

