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
#include <iostream>
#include <sstream>

#define SECS_PER_DAY 86400L

extern char cid_num[], cid_name[];

static char g_szLastLoginDate[9];

/* 
 * Enable the following line if you need remote.exe or to be able to launch
 * commands specified in remotes.dat.  This is disabled by default
 */
// #define ENABLE_REMOTE_VIA_SPECIAL_LOGINS

//
// Local functions
//

void CleanUserInfo()
{
    if ( okconf( &sess->thisuser ) )
    {
        setuconf( CONF_SUBS, sess->thisuser.GetLastSubConf(), 0 );
        setuconf( CONF_DIRS, sess->thisuser.GetLastDirConf(), 0 );
    }
    if ( sess->thisuser.GetLastSubNum() > sess->GetMaxNumberMessageAreas() )
    {
        sess->thisuser.SetLastSubNum( 0 );
    }
    if ( sess->thisuser.GetLastDirNum() > sess->GetMaxNumberFileAreas() )
    {
        sess->thisuser.SetLastDirNum( 0 );
    }
    if (usub[sess->thisuser.GetLastSubNum()].subnum != -1)
    {
        sess->SetCurrentMessageArea( sess->thisuser.GetLastSubNum() );
    }
    if (udir[sess->thisuser.GetLastDirNum()].subnum != -1)
    {
        sess->SetCurrentFileArea( sess->thisuser.GetLastDirNum() );
    }

}


bool random_screen( const char *mpfn )
{
    char szBuffer[ 255 ];
    sprintf( szBuffer, "%s%s%s", sess->pszLanguageDir, mpfn, ".0" );
    if ( WFile::Exists( szBuffer ) )
    {
        int nNumberOfScreens = 0;
        for ( int i = 0; i < 1000; i++ )
        {
            sprintf( szBuffer, "%s%s.%d", sess->pszLanguageDir, mpfn, i );
            if ( WFile::Exists( szBuffer ) )
            {
                nNumberOfScreens++;
            }
            else
            {
                break;
            }
        }
        sprintf( szBuffer, "%s%s.%d", sess->pszLanguageDir, mpfn, WWIV_GetRandomNumber( nNumberOfScreens ) );
        printfile( szBuffer );
        return true;
    }
    return false;
}


bool IsPhoneNumberUSAFormat( WUser *pUser )
{
    std::string country = pUser->GetCountry();
    return ( country == "USA" || country == "CAN" || country == "MEX" );
}


int GetNetworkOnlyStatus()
{
    int nNetworkOnly = 1;
    if (syscfg.netlowtime != syscfg.nethightime)
    {
        if (syscfg.nethightime > syscfg.netlowtime)
        {
            if ((timer() <= (syscfg.netlowtime * SECONDS_PER_MINUTE_FLOAT))
                || (timer() >= (syscfg.nethightime * SECONDS_PER_MINUTE_FLOAT)))
                nNetworkOnly = 0;
        }
        else
        {
            if ((timer() <= (syscfg.netlowtime * SECONDS_PER_MINUTE_FLOAT))
                && (timer() >= (syscfg.nethightime * SECONDS_PER_MINUTE_FLOAT)))
            {
                nNetworkOnly = 0;
            }
        }
    }
    else
    {
        nNetworkOnly = 0;
    }
    return nNetworkOnly;
}

int GetAnsiStatusAndShowWelcomeScreen( int nNetworkOnly )
{
    int ans = -1;
    if ( !nNetworkOnly && incom )
    {
        if ( sess->GetCurrentSpeed().length() > 0 )
	    {
            char szCurrentSpeed[ 81 ];
            strcpy( szCurrentSpeed, sess->GetCurrentSpeed().c_str() );
            sess->bout << "CONNECT " << strupr( szCurrentSpeed ) << "\r\n\r\n";
	    }
	    char szOSVersion[ 255 ];
        WWIV_GetOSVersion( szOSVersion, 250, true );
        sess->bout << "\r\nWWIV " << wwiv_version << "/" << szOSVersion << " " << beta_version << wwiv::endl;
        sess->bout << "Copyright (c) 1998-2004 WWIV Software Services." << wwiv::endl;
        sess->bout << "All Rights Reserved." << wwiv::endl;

        ans = check_ansi();
        char szFileName[ MAX_PATH ];
        sprintf( szFileName, "%s%s", sess->pszLanguageDir, WELCOME_ANS );
        if ( WFile::Exists( szFileName ) )
        {
            nl();
            if ( ans > 0 )
            {
                sess->thisuser.setStatusFlag( WUser::ansi );
                sess->thisuser.setStatusFlag( WUser::color );
                if ( !random_screen( WELCOME_NOEXT ) )
                {
                    printfile( WELCOME_ANS );
                }
            }
            else if ( ans == 0 )
            {
                printfile( WELCOME_MSG );
            }
        }
        else
        {
            if ( ans )
            {
                sprintf( szFileName, "%s%s.0", sess->pszLanguageDir, WELCOME_NOEXT );
                if ( WFile::Exists( szFileName ) )
                {
                    random_screen( WELCOME_NOEXT );
                }
                else
                {
                    printfile( WELCOME_MSG );
                }
            }
            else
            {
                printfile( WELCOME_MSG );
            }
        }
    }
    if ( curatr != 7 )
    {
        reset_colors();
    }
    return ans;
}


int ShowLoginAndGetUserNumber( int nNetworkOnly, char* pszUserName )
{
    char szUserName[ 255 ];

    nl();
    if ( nNetworkOnly )
    {
        sess->bout << "This time is reserved for net-mail ONLY.  Please try calling back again later.\r\n";
    }
    else
    {
        sess->bout << "Enter number or name or 'NEW'\r\n";
    }

    memset( &szUserName, 0, 255 );

    sess->bout << "NN: ";
    input( szUserName, 30 );
    StringTrim( szUserName );

    int nUserNumber = finduser( szUserName );
    if ( nUserNumber == 0 && szUserName[ 0 ] != '\0' )
    {
        sess->bout << "Searching...";
        bool abort = false, next = false;
        for ( int i = 1; i < status.users && nUserNumber == 0 && !hangup && !abort; i++ )
        {
            if ( i % 25 == 0 )  // changed from 15 since computers are faster now-a-days
            {
                sess->bout << ".";
            }
            int nTempUserNumber = smallist[i].number;
            sess->ReadCurrentUser( nTempUserNumber );
            if ( szUserName[ 0 ] == sess->thisuser.GetRealName()[ 0 ] )
            {
                char szTempUserName[ 255 ];
                strcpy( szTempUserName, sess->thisuser.GetRealName() );
                if ( wwiv::stringUtils::IsEquals( szUserName, strupr( szTempUserName ) ) &&
                     !sess->thisuser.isUserDeleted() )
                {
                    sess->bout << "|#5Do you mean " << sess->thisuser.GetUserNameAndNumber( i ) << "? ";
                    if ( yesno() )
                    {
                        nUserNumber = nTempUserNumber;
                    }
                }
            }
            checka( &abort, &next );
        }
    }
    strcpy( pszUserName, szUserName );
    return nUserNumber;
}


bool IsPhoneRequired()
{
    if ( ini_init( WWIV_INI, INI_TAG, NULL ) )
    {
        char *ss = NULL;
        if ( ( ss = ini_get( "NEWUSER_MIN", -1, NULL ) ) != NULL )
        {
            if ( wwiv::UpperCase<char>(ss[0]) == 'Y' )
            {
                return false;
            }
        }
        if ( ( ss = ini_get( "LOGON_PHONE", -1, NULL ) ) != NULL )
        {
            if ( wwiv::UpperCase<char>(ss[0]) == 'N' )
            {
                return false;
            }
        }
    }
    return true;
}

bool VerifyPhoneNumber()
{
    if ( IsPhoneNumberUSAFormat( &sess->thisuser ) || !sess->thisuser.GetCountry()[0] )
    {
        std::string phoneNumber;
        input_password( "PH: ###-###-", phoneNumber, 4 );

        if ( phoneNumber == &sess->thisuser.GetVoicePhoneNumber()[8] )
        {
            if ( phoneNumber.length() == 4 && phoneNumber[3] == '-' )
            {
                sess->bout << "\r\n!! Enter the LAST 4 DIGITS of your phone number ONLY !!\r\n\n";
            }
            return false;
        }
    }
    return true;
}


bool VerifyPassword()
{
    app->localIO->UpdateTopScreen();

    std::string password;
    input_password( "PW: ", password, 8 );

    return ( password == sess->thisuser.GetPassword() ) ? true : false;
}


bool VerifySysopPassword()
{
    std::string password;
    input_password( "SY: ", password, 20 );
    return ( password == syscfg.systempw ) ? true : false;
}


void DoFailedLoginAttempt()
{
    sess->thisuser.SetNumIllegalLogons( sess->thisuser.GetNumIllegalLogons( ) + 1 );
    sess->WriteCurrentUser( sess->usernum );
    sess->bout << "\r\n\aILLEGAL LOGON\a\r\n\n";

	std::stringstream logLine;
	logLine << "### ILLEGAL LOGON for " <<
             sess->thisuser.GetUserNameAndNumber( sess->usernum ) << " (" <<
             ctim( timer() ) << ")";
    sysoplog( "", false );
    sysoplog( logLine.str().c_str(), false );
    sysoplog( "", false );
    sess->usernum = 0;
}


void ExecuteWWIVNetworkRequest( const char *pszUserName )
{
    char szUserName[ 255 ];
    strcpy( szUserName, pszUserName );
    if (incom)
    {
        hangup = true;
        return;
    }

    app->statusMgr->Read();
    long lTime = time( NULL );
    switch ( sess->usernum )
    {
    case -2:
        {
			std::stringstream networkCommand;
			networkCommand << "NETWORK /B" << modem_speed << " /T" << lTime << " /F" << modem_flag;
            write_inst( INST_LOC_NET, 0, INST_FLAGS_NONE );
            ExecuteExternalProgram( networkCommand.str().c_str(), EFLAG_NONE );
            if ( app->GetInstanceNumber() != 1 )
            {
                send_inst_cleannet();
            }
            set_net_num( 0 );
        }
        break;
#ifdef ENABLE_REMOTE_VIA_SPECIAL_LOGINS
    case -3:
        {
			std::stringstream networkCommand;
			networkCommand << "REMOTE /B" << modem_speed << " /F" << modem_flag;
            ExecuteExternalProgram( networkCommand.str().c_str() , EFLAG_NONE );
        }
        break;
    case -4:
        {
            szUserName[8] = '\0';
            if (szUserName[0])
            {
				std::stringstream networkCommand;
				networkCommand << szUserName << " /B" << modem_speed << " /F" << modem_flag;
				std::stringstream remoteDatFileName;
				remoteDatFileName << syscfg.datadir << REMOTES_DAT;
				FILE* fp = fsh_open( remoteDatFileName.str().c_str(), "rt" );
                if ( fp )
                {
                    bool ok = false;
                    char szBuffer[ 255 ];
                    while ( !ok && fgets( szBuffer, 80, fp ) )
                    {
                        char* ss = strchr( szBuffer, '\n' );
                        if ( ss )
                        {
                            *ss = 0;
                        }
                        if ( wwiv::stringUtils::IsEqualsIgnoreCase( szBuffer, szUserName ) )
                        {
                            ok = true;
                        }
                    }
                    fsh_close( fp );
                    if ( ok )
                    {
						ExecuteExternalProgram( networkCommand.str().c_str(), EFLAG_NONE );
                    }
                }
            }
        }
        break;
#endif // ENABLE_REMOTE_VIA_SPECIAL_LOGINS
    }
    app->statusMgr->Read();
    hangup = true;
    app->comm->dtr( false );
    global_xx = false;
    Wait(1.0);
    app->comm->dtr( true );
    Wait(0.1);
    cleanup_net();
    imodem( false );
    hangup = true;
}


void LeaveBadPasswordFeedback( int ans )
{
    if ( ans > 0 )
    {
        sess->thisuser.setStatusFlag( WUser::ansi );
    }
    else
    {
        sess->thisuser.clearStatusFlag( WUser::ansi );
    }
    sess->bout << "|12Too many logon attempts!!\r\n\n";
    sess->bout << "|#9Would you like to leave Feedback to " << syscfg.sysopname << "? ";
    if ( yesno() )
    {
        nl();
        sess->bout << "What is your NAME or HANDLE? ";
        std::string tempName;
	    input1( tempName, 31, PROPER, true );
        if ( !tempName.empty() )
        {
            nl();
            sess->usernum = 1;
            sprintf( irt, "** Illegal logon feedback from %s", tempName.c_str() );
            sess->thisuser.SetName( tempName.c_str() );
            sess->thisuser.SetMacro( 0, "" );
            sess->thisuser.SetMacro( 1, "" );
            sess->thisuser.SetMacro( 2, "" );
            sess->thisuser.SetSl( syscfg.newusersl );
            sess->thisuser.SetScreenChars( 80 );
            if ( ans > 0 )
            {
                select_editor();
            }
            else
            {
                sess->thisuser.SetDefaultEditor( 0 );
            }
            sess->thisuser.SetNumEmailSent( 0 );
            bool bSaveAllowCC = sess->IsCarbonCopyEnabled();
            sess->SetCarbonCopyEnabled( false );
            email( 1, 0, true, 0, true );
            sess->SetCarbonCopyEnabled( bSaveAllowCC );
            if ( sess->thisuser.GetNumEmailSent() > 0 )
            {
                ssm( 1, 0, "Check your mailbox.  Someone forgot their password again!" );
            }
        }
    }
    sess->usernum = 0;
    hangup = 1;
}


void CheckCallRestrictions()
{
    if ( !hangup && sess->usernum > 0 && sess->thisuser.isRestrictionLogon() &&
        wwiv::stringUtils::IsEquals( date(), sess->thisuser.GetLastOn() ) &&
        sess->thisuser.GetTimesOnToday() > 0 )
    {
        nl();
        sess->bout << "|12Sorry, you can only logon once per day.\r\n";
        hangup = true;
    }
}


void DoCallBackVerification()
{
    int cbv = 0;
    if ( !sess->cbv.forced )
    {
        sess->bout << "|#1Callback verification now (y/N):|#0 ";
        if ( yesno() )
        {
            if ( sess->thisuser.GetCbv() & 4 )
            {
                printfile( CBV6_NOEXT );               // user explains and
                feedback( false );                     // makes excuses
            }
            cbv = callback();
        }
        else
        {
            cbv = 4;
        }
    }
    else
    {
        if ( sess->thisuser.GetCbv() & 4 )
        {
            printfile( CBV6_NOEXT );                 // user explains and
            feedback( false );                       // makes excuses
        }
        cbv = callback();
    }
    if ( cbv & 1 )
    {
        sess->thisuser.SetCbv( sess->thisuser.GetCbv() | 1 );
        if ( sess->thisuser.GetSl() < sess->cbv.sl )
        {
            sess->thisuser.SetSl( sess->cbv.sl );
        }
        if ( sess->thisuser.GetDsl() < sess->cbv.dsl )
        {
            sess->thisuser.SetDsl( sess->cbv.dsl );
        }
        sess->thisuser.SetArFlag( sess->cbv.ar );
        sess->thisuser.SetDarFlag( sess->cbv.dar );
        sess->thisuser.setExemptFlag( sess->cbv.exempt );
        sess->thisuser.setRestrictionFlag( sess->cbv.restrict );
        sess->WriteCurrentUser( sess->usernum );
    }
    else
    {
        sess->thisuser.SetCbv( sess->thisuser.GetCbv() | 4 );
        sess->WriteCurrentUser( sess->usernum );
    }
    if ( cbv & 2 )
    {
        if ( sess->cbv.longdistance )
        {
            printfile( CBV4_NOEXT );	// long distance verified
            pausescr();
            hangup = true;
        }
        else
        {
            printfile( CBV5_NOEXT );	// long distance no call
            pausescr();
        }
    }
    if ( cbv == 0 )
    {
        hangup = true;
    }
}


void getuser()
{
    char szUserName[ 255 ];
    write_inst( INST_LOC_GETUSER, 0, INST_FLAGS_NONE );

    int nNetworkOnly = GetNetworkOnlyStatus();
    int count = 0;
    bool ok = false;

    okmacro = false;
    sess->SetCurrentConferenceMessageArea( 0 );
    sess->SetCurrentConferenceFileArea( 0 );
    sess->SetEffectiveSl( syscfg.newusersl );
    sess->thisuser.SetStatus( 0 );

    int ans = GetAnsiStatusAndShowWelcomeScreen( nNetworkOnly );

    do
    {
        sess->usernum = ShowLoginAndGetUserNumber( nNetworkOnly, szUserName );

        if ( nNetworkOnly && sess->usernum != -2 )
        {
            if ( sess->usernum != -4 ||
                 !wwiv::stringUtils::IsEquals( szUserName, "DNM" ) )
            {
                sess->usernum = 0;
            }
        }
        if ( sess->usernum > 0 )
        {
            sess->ReadCurrentUser( sess->usernum );
            read_qscn( sess->usernum, qsc, false );
            if ( !set_language( sess->thisuser.GetLanguage() ) )
            {
                sess->thisuser.SetLanguage( 0 );
                set_language( 0 );
            }
            int nInstanceNumber;
            if ( sess->thisuser.GetSl() < 255 && user_online( sess->usernum, &nInstanceNumber ) )
            {
                sess->bout << "\r\n|12You are already online on instance " << nInstanceNumber << "!\r\n\n";
                continue;
            }
            ok = true;
            if ( guest_user )
            {
                logon_guest();
            }
            else
            {
                sess->SetEffectiveSl( syscfg.newusersl );
                if ( !VerifyPassword() )
                {
                    ok = false;
                }

                if ( ( syscfg.sysconfig & sysconfig_free_phone ) == 0 && IsPhoneRequired() )
                {
                    if ( !VerifyPhoneNumber() )
                    {
                        ok = false;
                    }
                }
                if ( sess->thisuser.GetSl() == 255 && incom && ok )
                {
                    if ( !VerifySysopPassword() )
                    {
                        ok = false;
                    }
                }
            }
            if ( ok )
            {
                sess->ResetEffectiveSl();
                changedsl();
            }
            else
            {
                DoFailedLoginAttempt();
            }
        }
        else if ( sess->usernum == 0 )
        {
            nl();
            if ( !nNetworkOnly )
            {
                sess->bout << "|12Unknown user.\r\n";
            }
        }
        else if ( sess->usernum == -1 )
        {
            write_inst( INST_LOC_NEWUSER, 0, INST_FLAGS_NONE );
            play_sdf( NEWUSER_NOEXT, false );
            newuser();
            ok = true;
        }
        else if ( sess->usernum == -2 || sess->usernum == -3 || sess->usernum == -4 )
        {
            ExecuteWWIVNetworkRequest( szUserName );
        }
    } while ( !hangup && !ok && ++count < 3 );

    if ( count >= 3 )
    {
        LeaveBadPasswordFeedback( ans );
    }

    okmacro = true;

    CheckCallRestrictions();

    if ( app->HasConfigFlag( OP_FLAGS_CALLBACK ) && ( sess->thisuser.GetCbv() & 1 ) == 0 )
    {
        DoCallBackVerification();
    }
}


void FixUserLinesAndColors()
{
    if ( sess->thisuser.GetNumExtended() > sess->max_extend_lines )
    {
        sess->thisuser.SetNumExtended( sess->max_extend_lines );
    }
    if ( sess->thisuser.GetColor( 8 ) == 0 || sess->thisuser.GetColor( 9 ) == 0 )
    {
        sess->thisuser.SetColor( 8, sess->newuser_colors[8] );
        sess->thisuser.SetColor( 9, sess->newuser_colors[9] );
    }
}

void UpdateUserStatsForLogin()
{
    strcpy( g_szLastLoginDate, date() );
    if ( wwiv::stringUtils::IsEquals( g_szLastLoginDate, sess->thisuser.GetLastOn() ) )
    {
        sess->thisuser.SetTimesOnToday( sess->thisuser.GetTimesOnToday() + 1 );
    }
    else
    {
        sess->thisuser.SetTimesOnToday( 1 );
        sess->thisuser.SetTimeOnToday( 0.0 );
        sess->thisuser.SetExtraTime( 0.0 );
        sess->thisuser.SetNumPostsToday( 0 );
        sess->thisuser.SetNumEmailSentToday( 0 );
        sess->thisuser.SetNumFeedbackSentToday( 0 );
    }
    sess->thisuser.SetNumLogons( sess->thisuser.GetNumLogons() + 1 );
    sess->SetCurrentMessageArea( 0 );
    sess->SetNumMessagesReadThisLogon( 0 );
    if ( udir[0].subnum == 0 && udir[1].subnum > 0 )
    {
        sess->SetCurrentFileArea( 1 );
    }
    else
    {
        sess->SetCurrentFileArea( 0 );
    }
    sess->SetMMKeyArea( WSession::mmkeyMessageAreas );
    if ( sess->GetEffectiveSl() != 255 && !guest_user )
    {
        app->statusMgr->Lock();
        ++status.callernum1;
        ++status.callstoday;
        app->statusMgr->Write();
    }
}

void PrintLogonFile()
{
    if ( !incom )
    {
        return;
    }
    play_sdf( LOGON_NOEXT, false );
    if ( !printfile( LOGON_NOEXT ) )
    {
        pausescr();
    }
}


void UpdateLastOnFileAndUserLog()
{
    char s1[181], szLastOnTxtFileName[ MAX_PATH ], szLogLine[ 255 ];
    long len;

    sprintf( szLogLine, "%ld: %s %s %s   %s - %d (%u)",
             status.callernum1,
             sess->thisuser.GetUserNameAndNumber( sess->usernum ),
             times(),
             fulldate(),
             sess->GetCurrentSpeed().c_str(),
             sess->thisuser.GetTimesOnToday(),
             app->GetInstanceNumber() );
    sprintf( szLastOnTxtFileName, "%s%s", syscfg.gfilesdir, LASTON_TXT );
    char *ss = get_file( szLastOnTxtFileName, &len );
    long pos = 0;
    if ( ss != NULL )
    {
        if ( !cs() )
        {
            for ( int i = 0; i < 4; i++ )
            {
                copy_line( s1, ss, &pos, len );
            }
        }
        int i = 1;
        do
        {
            copy_line( s1, ss, &pos, len );
            if ( s1[0] )
            {
                if ( i )
                {
                    sess->bout << "\r\n\n|#1Last few callers|#7: |#0\r\n\n";
                    if ( app->HasConfigFlag( OP_FLAGS_SHOW_CITY_ST ) &&
						 ( syscfg.sysconfig & sysconfig_extended_info ) )
                    {
						sess->bout << "|#2Number Name/Handle               Time  Date  City            ST Cty Modem    ##\r\n";
                    }
                    else
                    {
                        sess->bout << "|#2Number Name/Handle               Language   Time  Date  Speed                ##\r\n";
                    }
                    unsigned char chLine = ( okansi() ) ? 205 : '=';
                    sess->bout << "|#7" << charstr( 79, chLine ) << wwiv::endl;
                    i = 0;
                }
                sess->bout << s1;
				nl();
            }
        } while ( pos < len );
        nl( 2 );
        pausescr();
    }

    if ( sess->GetEffectiveSl() != 255 || incom )
    {
		sysoplog( "", false );
        sysoplog( stripcolors( szLogLine ), false );
		sysoplog( "", false );
        if ( cid_num[0] )
        {
            sysoplogf( "CID NUM : %s", cid_num );
        }
        if ( cid_name[0] )
        {
            sysoplogf( "CID NAME: %s", cid_name );
        }
        if ( app->HasConfigFlag( OP_FLAGS_SHOW_CITY_ST ) &&
			 ( syscfg.sysconfig & sysconfig_extended_info ) )
		{
            sprintf( szLogLine, "|#1%-6ld %-25.25s %-5.5s %-5.5s %-15.15s %-2.2s %-3.3s %-8.8s %2d\r\n",
                        status.callernum1,
                        sess->thisuser.GetUserNameAndNumber( sess->usernum ),
                        times(),
                        fulldate(),
                        sess->thisuser.GetCity(),
                        sess->thisuser.GetState(),
                        sess->thisuser.GetCountry(),
                        sess->GetCurrentSpeed().c_str(),
                        sess->thisuser.GetTimesOnToday() );
		}
        else
		{
            sprintf( szLogLine, "|#1%-6ld %-25.25s %-10.10s %-5.5s %-5.5s %-20.20s %2d\r\n",
                        status.callernum1,
                        sess->thisuser.GetUserNameAndNumber( sess->usernum ),
                        cur_lang_name,
                        times(),
                        fulldate(),
                        sess->GetCurrentSpeed().c_str(),
                        sess->thisuser.GetTimesOnToday() );
		}

        if ( sess->GetEffectiveSl() != 255 )
		{
            WFile userLog( syscfg.gfilesdir, USER_LOG );
            if ( userLog.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile,
                               WFile::shareUnknown, WFile::permReadWrite ) )
			{
                userLog.Seek( 0L, WFile::seekEnd );
                userLog.Write( szLogLine, strlen( szLogLine ) );
                userLog.Close();
            }
            WFile lastonFile( szLastOnTxtFileName );
            if ( lastonFile.Open( WFile::modeReadWrite | WFile::modeBinary |
                                  WFile::modeCreateFile | WFile::modeTruncate,
                                  WFile::shareUnknown, WFile::permReadWrite ) )
			{
				if ( ss != NULL )
				{
					// Need to ensure ss is not null here
					pos = 0;
					copy_line( s1, ss, &pos, len );
					for ( int i = 1; i < 8; i++ )
					{
						copy_line( s1, ss, &pos, len );
						strcat( s1, "\r\n" );
						lastonFile.Write( s1, strlen( s1 ) );
					}
				}
                lastonFile.Write( szLogLine, strlen( szLogLine ) );
                lastonFile.Close();
            }
        }
    }
    if ( ss != NULL )
	{
        BbsFreeMemory( ss );
	}
}


void CheckAndUpdateUserInfo()
{
    fsenttoday = 0;
    if ( sess->thisuser.GetBirthdayYear() == 0 )
    {
        sess->bout << "\r\nPlease enter the following information:\r\n";
        do
        {
            nl();
            input_age( &sess->thisuser );
            nl();
            bprintf( "%02d/%02d/%02d -- Correct? ",
                     sess->thisuser.GetBirthdayMonth(),
                     sess->thisuser.GetBirthdayDay(),
                     sess->thisuser.GetBirthdayYear() );
            if ( !yesno() )
            {
                sess->thisuser.SetBirthdayYear( 0 );
            }
        } while ( !hangup && sess->thisuser.GetBirthdayYear() == 0 );
    }

    if ( !sess->thisuser.GetRealName()[0] )
    {
        input_realname();
    }
    if ( !( syscfg.sysconfig & sysconfig_extended_info ) )
    {
        return;
    }
    if ( !sess->thisuser.GetStreet()[0] )
    {
        input_street();
    }
    if ( !sess->thisuser.GetCity()[0] )
    {
        input_city();
    }
    if ( !sess->thisuser.GetState()[0] )
    {
        input_state();
    }
    if ( !sess->thisuser.GetCountry()[0] )
    {
        input_country();
    }
    if ( !sess->thisuser.GetZipcode()[0] )
    {
        input_zipcode();
    }
    if ( !sess->thisuser.GetDataPhoneNumber()[0] )
    {
        input_dataphone();
    }
    if ( sess->thisuser.GetComputerType() == -1 )
    {
        input_comptype();
    }

    if ( !app->HasConfigFlag( OP_FLAGS_USER_REGISTRATION ) )
    {
        return;
    }

    if ( sess->thisuser.GetRegisteredDateNum() == 0 )
    {
        return;
    }

    time_t lTime = time( NULL );
    if ((sess->thisuser.GetExpiresDateNum() < static_cast<unsigned long>(lTime + 30 * SECS_PER_DAY))
        && (sess->thisuser.GetExpiresDateNum() > static_cast<unsigned long>(lTime + 10 * SECS_PER_DAY)))
    {
        sess->bout << "Your registration expires in " <<
                      static_cast<int>((sess->thisuser.GetExpiresDateNum() - lTime) / SECS_PER_DAY) <<
                      "days";
    }
    else if ( ( sess->thisuser.GetExpiresDateNum() > static_cast<unsigned long>( lTime ) ) &&
              ( sess->thisuser.GetExpiresDateNum() < static_cast<unsigned long>( lTime + 10 * SECS_PER_DAY ) ))
    {
        if ( static_cast<int>( ( sess->thisuser.GetExpiresDateNum() - lTime ) / static_cast<unsigned long>( SECS_PER_DAY ) ) > 1 )
        {
            sess->bout << "|#6Your registration expires in " <<
                        static_cast<int>((sess->thisuser.GetExpiresDateNum() - lTime) / static_cast<unsigned long>( SECS_PER_DAY ) )
                        << " days";
        }
        else
        {
            sess->bout << "|#6Your registration expires in " <<
                            static_cast<int>( ( sess->thisuser.GetExpiresDateNum() - lTime ) / static_cast<unsigned long>( 3600L ) )
                            << " hours.";
        }
        nl( 2 );
        pausescr();
    }
    if ( sess->thisuser.GetExpiresDateNum() < static_cast<unsigned long>( lTime ) )
    {
        if ( !so() )
        {
            if ( sess->thisuser.GetSl() > syscfg.newusersl ||
                 sess->thisuser.GetDsl() > syscfg.newuserdsl )
            {
                sess->thisuser.SetSl( syscfg.newusersl );
                sess->thisuser.SetDsl( syscfg.newuserdsl );
                sess->thisuser.SetExempt( 0 );
                ssm( 1, 0, "%s%s", sess->thisuser.GetUserNameAndNumber( sess->usernum ), "'s registration has expired." );
                sess->WriteCurrentUser( sess->usernum );
                sess->ResetEffectiveSl();
                changedsl();
            }
        }
        sess->bout << "|#6Your registration has expired.\r\n\n";
        pausescr();
    }
}


void DisplayUserLoginInformation()
{
    char s1[255];
    nl();

    sess->bout << "|#9Name/Handle|#0....... |#2" << sess->thisuser.GetUserNameAndNumber( sess->usernum ) << wwiv::endl;
    sess->bout << "|#9Internet Address|#0.. |#2";
    if ( check_inet_addr(sess->thisuser.GetEmailAddress() ) )
    {
        sess->bout << sess->thisuser.GetEmailAddress() << wwiv::endl;
    }
    else if ( !sess->internetEmailName.empty() )
    {
        sess->bout << ( sess->IsInternetUseRealNames() ? sess->thisuser.GetRealName() : sess->thisuser.GetName() )
                    << "<" << sess->internetFullEmailAddress << ">\r\n";
    }
    else
    {
        sess->bout << "None.\r\n";
    }
    sess->bout << "|#9Time allowed on|#0... |#2" << static_cast<int>( ( nsl() + 30 ) / SECONDS_PER_MINUTE_FLOAT ) << wwiv::endl;
    if ( sess->thisuser.GetNumIllegalLogons() > 0 )
    {
        sess->bout << "|#9Illegal logons|#0.... |#2" << sess->thisuser.GetNumIllegalLogons() << wwiv::endl;
    }
    if ( sess->thisuser.GetNumMailWaiting() > 0 )
    {
        sess->bout << "|#9Mail waiting|#0...... |#2" << sess->thisuser.GetNumMailWaiting() << wwiv::endl;
    }
    if ( sess->thisuser.GetTimesOnToday() == 1 )
    {
        sess->bout << "|#9Date last on|#0...... |#2" << sess->thisuser.GetLastOn() << wwiv::endl;
    }
    else
    {
        sess->bout << "|#9Times on today|#0.... |#2" << sess->thisuser.GetTimesOnToday() << wwiv::endl;
    }
    sess->bout << "|#9Sysop currently|#0... |#2";
    if ( sysop2() )
    {
        sess->bout << "Available\r\n";
    }
    else
    {
        sess->bout << "NOT Available\r\n";
    }

    sess->bout << "|#9System is|#0......... |#2WWIV " << wwiv_version << beta_version << "  " << wwiv::endl;

    /////////////////////////////////////////////////////////////////////////
    app->statusMgr->Read();
    for ( int i = 0; i < sess->GetMaxNetworkNumber(); i++ )
    {
        if ( net_networks[i].sysnum )
        {
            sprintf( s1, "|#9%s node|#0%s|#2 @%u", net_networks[i].name, charstr(13 - strlen(net_networks[i].name), '.'), net_networks[i].sysnum );
            if ( i )
            {
                sess->bout << s1;
				nl();
            }
            else
            {
  				int i1;
                for ( i1 = strlen(s1); i1 < 26; i1++ )
                {
                    s1[i1] = ' ';
                }
                s1[i1] = '\0';
                sess->bout << s1 << "(net" << status.net_version << ")\r\n";
            }
        }
    }

    char szOSVersion[100];
    WWIV_GetOSVersion( szOSVersion, 100, true );
    sess->bout << "|#9OS|#0................ |#2" << szOSVersion << wwiv::endl;

    sess->bout << "|#9Instance|#0.......... |#2" << app->GetInstanceNumber() << "\r\n\n";
    if ( sess->thisuser.GetForwardUserNumber() )
    {
        if ( sess->thisuser.GetForwardSystemNumber() != 0 )
        {
            set_net_num( sess->thisuser.GetForwardNetNumber() );
            if ( !valid_system( sess->thisuser.GetForwardSystemNumber() ) )
            {
                sess->thisuser.SetForwardUserNumber( 0 );
                sess->thisuser.SetForwardSystemNumber( 0 );
                sess->bout << "Forwarded to unknown system; forwarding reset.\r\n";
            }
            else
            {
                sess->bout << "Mail set to be forwarded to ";
                if ( sess->GetMaxNetworkNumber() > 1 )
                {
                    sess->bout << "#" << sess->thisuser.GetForwardUserNumber()
							   << " @"
							   << sess->thisuser.GetForwardSystemNumber()
							   << "." << sess->GetNetworkName() << "."
							   << wwiv::endl;
                }
                else
                {
                    sess->bout << "#" << sess->thisuser.GetForwardUserNumber() << " @"
							   << sess->thisuser.GetForwardSystemNumber() << "." << wwiv::endl;
                }
            }
        }
        else
        {
            if ( sess->thisuser.GetForwardUserNumber() == 65535 )
            {
                sess->bout << "Your mailbox is closed.\r\n\n";
            }
            else
            {
                sess->bout << "Mail set to be forwarded to #" << sess->thisuser.GetForwardUserNumber() << wwiv::endl;
            }
        }
    }
    else if ( sess->thisuser.GetForwardSystemNumber() != 0 )
    {
		char szInternetEmailAddress[ 255 ];
        read_inet_addr( szInternetEmailAddress, sess->usernum );
        sess->bout << "Mail forwarded to Internet "<< szInternetEmailAddress << ".\r\n";
    }
    if ( sess->IsTimeOnlineLimited() )
    {
        sess->bout << "\r\n|13Your on-line time is limited by an external event.\r\n\n";
    }
}

void LoginCheckForNewMail()
{
    sess->bout << "|#9Scanning for new mail... ";
    if ( sess->thisuser.GetNumMailWaiting() > 0 )
    {
        int nNumNewMessages = check_new_mail( sess->usernum );
        if ( nNumNewMessages )
        {
            sess->bout << "|#9You have |#2" << nNumNewMessages << "|#9 new message(s).\r\n\r\n" <<
					      "|#9Read your mail now? ";
            if ( noyes() )
            {
                readmail( 1 );
            }
        }
        else
        {
            sess->bout << "|#9You have |#2" << sess->thisuser.GetNumMailWaiting() << "|#9 old message(s) in your mailbox.\r\n";
        }
    }
    else
    {
        sess->bout << " |#9No mail found.\r\n";
    }
}

void CheckUserForVotingBooth()
{
    if (!sess->thisuser.isRestrictionVote() && sess->GetEffectiveSl() > syscfg.newusersl )
    {
        for (int i = 0; i < 20; i++)
        {
            if ( questused[i] && sess->thisuser.GetVote( i ) == 0 )
            {
                nl();
                sess->bout << "|#9You haven't voted yet.\r\n";
                return;
            }
        }
    }
}

void logon()
{
    if (sess->usernum < 1)
    {
        hangup = true;
        return;
    }
	sess->SetUserOnline( true );
    write_inst( INST_LOC_LOGON, 0, INST_FLAGS_NONE );
    get_user_ppp_addr();
    get_next_forced_event();
    reset_colors();
    ansic( 0 );
    ClearScreen();

    FixUserLinesAndColors();

    UpdateUserStatsForLogin();

    PrintLogonFile();

    UpdateLastOnFileAndUserLog();

    read_automessage();
    timeon = timer();
    app->localIO->UpdateTopScreen();
    nl( 2 );
    pausescr();
    if ( syscfg.logon_c[0] )
	{
        nl();
        char szCommand[ MAX_PATH ];
        stuff_in( szCommand, syscfg.logon_c, create_chain_file(), "", "", "", "" );
        ExecuteExternalProgram( szCommand, app->GetSpawnOptions( SPWANOPT_LOGON ) );
        nl( 2 );
    }

    DisplayUserLoginInformation();

    CheckAndUpdateUserInfo();

    app->localIO->UpdateTopScreen();
    app->read_subs();
    rsm( sess->usernum, &sess->thisuser, true );

    LoginCheckForNewMail();

    if ( sess->thisuser.GetNewScanDateNumber() )
    {
        nscandate = sess->thisuser.GetNewScanDateNumber();
    }
    else
    {
        nscandate = sess->thisuser.GetLastOnDateNumber();
    }
    batchtime = 0.0;
    sess->numbatchdl = sess->numbatch = 0;

    CheckUserForVotingBooth();

    if ( ( incom || sysop1() ) && sess->thisuser.GetSl() < 255 )
    {
        broadcast( "%s Just logged on!", sess->thisuser.GetName() );
    }
    setiia( 90 );

	// New Message Scan
	if ( sess->IsNewScanAtLogin() )
	{
		sess->bout << "\r\n|10Scan All Message Areas For New Messages? ";
		if ( yesno() )
		{
			NewMsgsAllConfs();
		}
	}

    // Handle case of first conf with no subs avail
    if ( usub[0].subnum == -1 && okconf( &sess->thisuser ) )
    {
        for (sess->SetCurrentConferenceMessageArea( 0 ); (sess->GetCurrentConferenceMessageArea() < subconfnum) && (uconfsub[sess->GetCurrentConferenceMessageArea()].confnum != -1); sess->SetCurrentConferenceMessageArea( sess->GetCurrentConferenceMessageArea() + 1 ) )
        {
            setuconf( CONF_SUBS, sess->GetCurrentConferenceMessageArea(), -1 );
            if ( usub[0].subnum != -1 )
            {
                break;
            }
        }
        if ( usub[0].subnum == -1 )
        {
            sess->SetCurrentConferenceMessageArea( 0 );
            setuconf( CONF_SUBS, sess->GetCurrentConferenceMessageArea(), -1 );
        }
    }
    g_preloaded = false;

    if ( app->HasConfigFlag( OP_FLAGS_USE_FORCESCAN ) )
    {
        int nNextSubNumber = 0;
        if ( sess->thisuser.GetSl() < 255 )
        {
            forcescansub = true;
            qscan( sess->GetForcedReadSubNumber(), &nNextSubNumber );
            forcescansub = false;
        }
        else
        {
            qscan( sess->GetForcedReadSubNumber(), &nNextSubNumber );
        }
    }
    CleanUserInfo();
}


void logoff()
{
    mailrec m;

    if ( incom )
    {
        play_sdf( LOGOFF_NOEXT, false );
    }

    if ( sess->usernum > 0 )
    {
        if ( ( incom || sysop1() ) && sess->thisuser.GetSl() < 255 )
        {
            broadcast( "%s Just logged off!", sess->thisuser.GetName() );
        }
    }
    setiia(90);
    app->comm->dtr( false );
    hangup = true;
    if (sess->usernum < 1)
    {
        return;
    }

    std::string text = "  Logged Off At ";
    text += times();
    if ( sess->GetEffectiveSl() != 255 || incom )
    {
		sysoplog( "", false );
        sysoplog( stripcolors( text.c_str() ), false );
    }
    sess->thisuser.SetLastBaudRate( modem_speed );

	// put this back here where it belongs... (not sure why it te
    sess->thisuser.SetLastOn( g_szLastLoginDate );

    sess->thisuser.SetNumIllegalLogons( 0 );
    if ( ( timer() - timeon ) < -30.0 )
    {
        timeon -= HOURS_PER_DAY_FLOAT * SECONDS_PER_DAY_FLOAT;
    }
    double ton = timer() - timeon;
    sess->thisuser.SetTimeOn( sess->thisuser.GetTimeOn() + static_cast<float>( ton ) );
    sess->thisuser.SetTimeOnToday( sess->thisuser.GetTimeOnToday() + static_cast<float>( ton - extratimecall ) );
    app->statusMgr->Lock();
    status.activetoday = status.activetoday + static_cast<unsigned short>( ton / MINUTES_PER_HOUR_FLOAT );
    app->statusMgr->Write();
    if (g_flags & g_flag_scanned_files)
    {
        sess->thisuser.SetNewScanDateNumber( sess->thisuser.GetLastOnDateNumber() );
    }
    long lTime = time( NULL );
    sess->thisuser.SetLastOnDateNumber( lTime );
    sysoplogfi( false, "Read: %lu   Time on: %u", sess->GetNumMessagesReadThisLogon(), static_cast<int>( ( timer() - timeon ) / MINUTES_PER_HOUR_FLOAT ) );
    if (mailcheck)
    {
		WFile* pFileEmail = OpenEmailFile( true );
        WWIV_ASSERT( pFileEmail );
		if ( pFileEmail->IsOpen() )
        {
            sess->thisuser.SetNumMailWaiting( 0 );
			int t = static_cast< int >( pFileEmail->GetLength() / sizeof( mailrec ) );
            int r = 0;
            int w = 0;
            while ( r < t )
            {
				pFileEmail->Seek( static_cast<long>( sizeof( mailrec ) ) * static_cast<long>( r ), WFile::seekBegin );
				pFileEmail->Read( &m, sizeof( mailrec ) );
                if ( m.tosys != 0 || m.touser != 0 )
                {
                    if ( m.tosys == 0 && m.touser == sess->usernum )
                    {
                        if ( sess->thisuser.GetNumMailWaiting() != 255 )
                        {
                            sess->thisuser.SetNumMailWaiting( sess->thisuser.GetNumMailWaiting() + 1 );
                        }
                    }
                    if ( r != w )
                    {
						pFileEmail->Seek( static_cast<long>( sizeof( mailrec ) ) * static_cast<long>( w ), WFile::seekBegin );
						pFileEmail->Write( &m, sizeof( mailrec ) );
                    }
                    ++w;
                }
                ++r;
            }
            if (r != w)
            {
                m.tosys = 0;
                m.touser = 0;
                for ( int w1 = w; w1 < r; w1++ )
                {
					pFileEmail->Seek( static_cast<long>( sizeof( mailrec ) ) * static_cast<long>( w1 ), WFile::seekBegin );
					pFileEmail->Write( &m, sizeof( mailrec ) );
                }
            }
			pFileEmail->SetLength( static_cast<long>( sizeof( mailrec ) ) * static_cast<long>( w ) );
            app->statusMgr->Lock();
            status.filechange[filechange_email]++;
            app->statusMgr->Write();
			pFileEmail->Close();
			delete pFileEmail;
        }
    }
    else
    {
        // re-calculate mail waiting'
        WFile *pFileEmail = OpenEmailFile( false );
		WWIV_ASSERT( pFileEmail );
		if ( pFileEmail->IsOpen() )
        {
			int nTotalEmailMessages = static_cast<int>( pFileEmail->GetLength() / sizeof( mailrec ) );
            sess->thisuser.SetNumMailWaiting( 0 );
            for ( int i = 0; i < nTotalEmailMessages; i++ )
            {
				pFileEmail->Read( &m, sizeof( mailrec ) );
                if ( m.tosys == 0 && m.touser == sess->usernum )
                {
                    if ( sess->thisuser.GetNumMailWaiting() != 255 )
                    {
                        sess->thisuser.SetNumMailWaiting( sess->thisuser.GetNumMailWaiting() + 1 );
                    }
                }
            }
			pFileEmail->Close();
			delete pFileEmail;
        }
    }
    if ( smwcheck )
    {
        WFile smwFile( syscfg.datadir, SMW_DAT );
        if ( smwFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite ) )
        {
            int t = static_cast<int>( smwFile.GetLength() / sizeof( shortmsgrec ) );
            int r = 0;
            int w = 0;
            while ( r < t )
            {
				shortmsgrec sm;
                smwFile.Seek( r * sizeof( shortmsgrec ), WFile::seekBegin );
                smwFile.Read( &sm, sizeof( shortmsgrec ) );
                if ( sm.tosys != 0 || sm.touser != 0 )
                {
                    if ( sm.tosys == 0 && sm.touser == sess->usernum )
                    {
                        sess->thisuser.setStatusFlag( WUser::SMW );
                    }
                    if ( r != w )
                    {
                        smwFile.Seek( w * sizeof( shortmsgrec ), WFile::seekBegin );
                        smwFile.Write( &sm, sizeof( shortmsgrec ) );
                    }
                    ++w;
                }
                ++r;
            }
            smwFile.SetLength( w * sizeof( shortmsgrec ) );
            smwFile.Close();
        }
    }
    if ( sess->usernum == 1 )
    {
        fwaiting = sess->thisuser.GetNumMailWaiting();
    }
    sess->WriteCurrentUser( sess->usernum );
    write_qscn( sess->usernum, qsc, false );
    remove_from_temp( "*.*", syscfgovr.tempdir, false );
    remove_from_temp( "*.*", syscfgovr.batchdir, false );
    if ( sess->numbatch && ( sess->numbatch != sess->numbatchdl ) )
    {
        for ( int i = 0; i < sess->numbatch; i++ )
        {
            if ( !batch[i].sending )
            {
                didnt_upload( i );
            }
        }
    }
    sess->numbatch = sess->numbatchdl = 0;
}


void logon_guest()
{
    nl( 2 );
    input_ansistat();

    printfile( GUEST_NOEXT );
    pausescr();

    std::string userName, reason;
    int count = 0;
    do
    {
        nl();
        sess->bout << "|#5Enter your real name : ";
        input( userName, 25, true );
        nl();
        sess->bout << "|#5Purpose of your call?\r\n";
        input( reason, 79, true );
        if ( !userName.empty() && !reason.empty() )
        {
            break;
        }
        count++;
    } while ( !hangup && count < 3 );

    if ( count >= 3 )
    {
        printfile( REJECT_NOEXT );
        ssm( 1, 0, "Guest Account failed to enter name and purpose" );
        hangup = true;
    }
    else
    {
        ssm( 1, 0, "Guest Account accessed by %s on %s for", userName.c_str(), times() );
        ssm( 1, 0, reason.c_str() );
    }
}

