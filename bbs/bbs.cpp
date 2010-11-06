// $Header$
/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2010, WWIV Software Services             */
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

#define _DEFINE_GLOBALS_ 
#include "wwiv.h"
#undef _DEFINE_GLOBALS_

#if defined ( __APPLE__ ) && !defined ( __unix__ )
#define __unix__ 1
#endif

#include <memory>
#include "bbs.h"
#include "unittests.h"

#if defined( _WIN32 )
#include "InternalTelnetServer.h"
#include "Wiot.h"
#endif // _WIN32

#include "menu.h" // for mainmenu

static bool bUsingPppProject = true;
extern time_t last_time_c;
static WApplication *app;
static WSession* sess;


//////////////////////////////////////////////////////////////////////////////
//
// Implementation
//
//
//


const int WApplication::exitLevelOK          = 0;
const int WApplication::exitLevelNotOK       = 1;
const int WApplication::exitLevelQuit        = 2;

const int WApplication::shutdownNone         = 0;
const int WApplication::shutdownThreeMinutes = 1;
const int WApplication::shutdownTwoMinutes   = 2;
const int WApplication::shutdownOneMinute    = 3;
const int WApplication::shutdownImmediate    = 4;


WApplication* GetApplication() 
{
    return app;
}


WSession* GetSession()
{
    return sess;
}


StatusMgr* WApplication::GetStatusManager()
{
    return statusMgr;
}


WUserManager* WApplication::GetUserManager()
{
    return userManager;
}


#if !defined ( __unix__ )
void WApplication::GetCaller()
{
    GetSession()->SetMessageAreaCacheNumber( 0 );
    GetSession()->SetFileAreaCacheNumber( 0 );
    SetShutDownStatus( WApplication::shutdownNone );
    wfc_init();
    GetSession()->remoteIO()->ClearRemoteInformation();
    frequent_init();
    if (GetSession()->wfc_status == 0)
    {
        GetSession()->localIO()->LocalCls();
    }
    imodem( false );
    GetSession()->usernum = 0;
    SetWfcStatus( 0 );
    write_inst( INST_LOC_WFC, 0, INST_FLAGS_NONE );
    GetSession()->ReadCurrentUser( 1 );
    read_qscn( 1, qsc, false );
    GetSession()->usernum = 1;
    GetSession()->ResetEffectiveSl();
    fwaiting = GetSession()->GetCurrentUser()->GetNumMailWaiting();
    if ( GetSession()->GetCurrentUser()->IsUserDeleted() )
    {
        GetSession()->GetCurrentUser()->SetScreenChars( 80 );
        GetSession()->GetCurrentUser()->SetScreenLines( 25 );
    }
    GetSession()->screenlinest = defscreenbottom + 1;

    int lokb = doWFCEvents();

    if ( lokb )
    {
        modem_speed = ( ok_modem_stuff ) ? modem_i->defl.modem_speed : 14400;
    }

    GetSession()->using_modem = incom;
    if ( lokb == 2 )
    {
        GetSession()->using_modem = -1;
    }

    okskey = true;
    GetSession()->localIO()->LocalCls();
    GetSession()->localIO()->LocalPrintf( "%s %s ...\r\n",
                                ( ( modem_mode == mode_fax ) ? "Fax connection at" : "Logging on at" ),
                                GetSession()->GetCurrentSpeed().c_str() );
    SetWfcStatus( 0 );
}
#elif !defined( __APPLE__ )
void wfc_screen() {}
void wfc_cls() {}
#endif


int WApplication::doWFCEvents()
{
    char ch;
    int lokb;
    static int mult_time;

    std::auto_ptr<WStatus> pStatus(GetStatusManager()->GetStatus());
    do
    {
        write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
        set_net_num( 0 );
        bool any = false;
        SetWfcStatus( 1 );
        if ( !wwiv::stringUtils::IsEquals( date(), pStatus->GetLastDate() ) )
        {
            if ( ( GetSession()->GetBeginDayNodeNumber() == 0 ) || ( m_nInstance == GetSession()->GetBeginDayNodeNumber() ) )
            {
                cleanup_events();
                holdphone( true );
                beginday( true );
                holdphone( false );
                wfc_cls();
            }
        }

        if (!do_event)
        {
            check_event();
        }

        while (do_event)
        {
            run_event(do_event - 1);
            check_event();
            any = true;
        }

        lokb = 0;
        GetSession()->SetCurrentSpeed( "KB" );
        time_t lCurrentTime = time( NULL );
        if ( !any && (((rand() % 8000) == 0) || (lCurrentTime - last_time_c > 1200)) &&
            (net_sysnum) && (ok_modem_stuff || bUsingPppProject) &&
            ( this->flags & OP_FLAGS_NET_CALLOUT ) )
        {
            lCurrentTime = last_time_c;
            attempt_callout();
            any = true;
        }
        wfc_screen();
        okskey = false;
        if (GetSession()->localIO()->LocalKeyPressed())
        {
            SetWfcStatus( 0 );
            GetSession()->ReadCurrentUser( 1 );
            read_qscn(1, qsc, false);
            fwaiting = GetSession()->GetCurrentUser()->GetNumMailWaiting();
            SetWfcStatus( 1 );
            ch = wwiv::UpperCase<char>( GetSession()->localIO()->getchd1() );
            if (!ch)
            {
                ch = GetSession()->localIO()->getchd1();
                GetSession()->localIO()->skey(ch);
                ch=0;
            }
        }
        else
        {
            ch=0;
            giveup_timeslice();
        }
        if (ch)
        {
            SetWfcStatus( 2 );
            any = true;
            okskey = true;
            resetnsp();
            GetSession()->localIO()->SetCursor( WLocalIO::cursorNormal );
            switch ( ch )
            {
            // Local Logon
            case SPACE:
                lokb = this->LocalLogon();
                break;
                // Reset User Records
            case '=':
                if ( AllowLocalSysop() )
                {
                    holdphone( true );
                    reset_files();
                    holdphone( false );
                }
                break;
                // Show WFC Menu
            case '?':
                if ( AllowLocalSysop() )
                {
                    std::string helpFileName = SWFC_NOEXT;
                    char chHelp = ESC;
                    do
                    {
                        GetSession()->localIO()->LocalCls();
                        GetSession()->bout.NewLine();
                        printfile( helpFileName.c_str() );
                        chHelp = getkey();
                        helpFileName = ( helpFileName == SWFC_NOEXT ) ? SONLINE_NOEXT : SWFC_NOEXT;
                    } while ( chHelp != SPACE && chHelp != ESC );
                }
                break;
                // Force Network Callout
            case '/':
                if ( net_sysnum && AllowLocalSysop() && ( ok_modem_stuff || bUsingPppProject ) )
                {
                    force_callout( 0 );
                }
                break;
                // War Dial Connect
            case '.':
                if ( net_sysnum && AllowLocalSysop() && ( ok_modem_stuff || bUsingPppProject ) )
                {
                    force_callout( 1 );
                }
                break;

                // Fast Net Callout from WFC
                // replace '50' with your node number on your primary network (net[0])
                // replace '1160' with your primary connection on the primary network
            case '*':
                if ( ok_modem_stuff && net_sysnum == 50 &&  GetInstanceNumber() == 1)
                {
                    do_callout(1160);
                }
                break;
                // Run MenuEditor
            case '!':
                if ( AllowLocalSysop() )
                {
                    holdphone( true );
                    EditMenus();
                    holdphone( false );
                }
                break;
                // Print NetLogs
            case ',':
                if ( net_sysnum > 0 || GetSession()->GetMaxNetworkNumber() > 1 && AllowLocalSysop() )
                {
                    GetSession()->localIO()->LocalGotoXY( 2, 23 );
                    GetSession()->bout << "|#9(|#2Q|#9=|#2Quit|#9) Display Which NETDAT Log File (|#10|#9-|#12|#9): ";
                    ch = onek( "Q012" );
                    switch ( ch )
                    {
                    case '0':
                    case '1':
                    case '2':
						{
							char szNetDatFileName[ MAX_PATH ];
							snprintf( szNetDatFileName, sizeof( szNetDatFileName ), "netdat%c.log", ch );
							print_local_file( szNetDatFileName, "" );
						}
                        break;
                    }
                }
                break;
                // Net List
            case '`':
                if ( net_sysnum && AllowLocalSysop() )
                {
                    holdphone( true );
                    print_net_listing( true );
                    holdphone( false );
                }
                break;
                // [TAB] Instance Editor
            case TAB:
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    holdphone( true );
                    instance_edit();
                    holdphone( false );
                }
                break;
                // [ESC] Quit the BBS
            case ESC:
		        GetSession()->localIO()->LocalGotoXY( 2, 23 );
                GetSession()->bout << "|#7Exit the BBS? ";
                if ( yesno() )
                {
                    QuitBBS();
                }
                GetSession()->localIO()->LocalCls();
                break;
                // Answer Phone
            case 'A':
                if ( !ok_modem_stuff )
                {
                    break;
                }
				GetSession()->localIO()->LocalGotoXY( 2, 23 );
                answer_phone();
                break;
                // BoardEdit
            case 'B':
                if ( AllowLocalSysop() )
                {
                    write_inst( INST_LOC_BOARDEDIT, 0, INST_FLAGS_NONE );
                    holdphone( true );
                    boardedit();
                    cleanup_net();
                    holdphone( false );
                }
                break;
                // ChainEdit
            case 'C':
                if ( AllowLocalSysop() )
                {
                    write_inst( INST_LOC_CHAINEDIT, 0, INST_FLAGS_NONE );
                    holdphone( true );
                    chainedit();
                    holdphone( false );
                }
                break;
                // DirEdit
            case 'D':
                if ( AllowLocalSysop() )
                {
                    write_inst( INST_LOC_DIREDIT, 0, INST_FLAGS_NONE );
                    holdphone( true );
                    dlboardedit();
                    holdphone( false );
                }
                break;
                // Send Email
            case 'E':
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    GetSession()->usernum = 1;
                    holdphone( true );
                    GetSession()->bout << "|#1Send Email:";
                    send_email();
                    GetSession()->WriteCurrentUser( 1 );
                    cleanup_net();
                    holdphone( false );
                }
                break;
                // GfileEdit
            case 'G':
                if ( AllowLocalSysop() )
                {
                    write_inst( INST_LOC_GFILEEDIT, 0, INST_FLAGS_NONE );
                    holdphone( true );
                    gfileedit();
                    holdphone( false );
                }
                break;
                // EventEdit
            case 'H':
                if ( AllowLocalSysop() )
                {
                    write_inst(INST_LOC_EVENTEDIT, 0, INST_FLAGS_NONE);
                    holdphone( true );
                    eventedit();
                    holdphone( false );
                }
                break;
                // InitVotes
            case 'I':
                if ( AllowLocalSysop() )
                {
					wfc_cls();
                    write_inst( INST_LOC_VOTEEDIT, 0, INST_FLAGS_NONE );
                    holdphone( true );
                    ivotes();
                    holdphone( false );
                }
                break;
                // ConfEdit
            case 'J':
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    holdphone( true );
                    edit_confs();
                    holdphone( false );
                }
                break;
                // SendMailFile
            case 'K':
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    GetSession()->usernum = 1;
                    holdphone( true );
                    GetSession()->bout << "|#1Send any Text File in Email:\r\n\n|#2Filename: ";
                    std::string buffer;
                    input( buffer, 50 );
                    LoadFileIntoWorkspace( buffer.c_str(), false );
                    send_email();
                    GetSession()->WriteCurrentUser( 1 );
                    cleanup_net();
                    holdphone( false );
                }
                break;
                // Print Log Daily logs
            case 'L':
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    WStatus *pStatus = GetStatusManager()->GetStatus();
                    char szSysopLogFileName[ MAX_PATH ];
                    GetSysopLogFileName( date(), szSysopLogFileName );
                    print_local_file( szSysopLogFileName, pStatus->GetLogFileName() );
                    delete pStatus;
                }
                break;
                // Read User Mail
            case 'M':
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    GetSession()->usernum = 1;
                    holdphone( true );
                    readmail( 0 );
                    GetSession()->WriteCurrentUser( 1 );
                    cleanup_net();
                    holdphone( false );
                }
                break;
                // Print Net Log
            case 'N':
                if ( AllowLocalSysop() )
		        {
                    wfc_cls();
                    print_local_file( "net.log", "netdat*.log" );
		        }
                break;
                // EditTextFile
            case 'O':
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    holdphone( true );
                    write_inst( INST_LOC_TEDIT, 0, INST_FLAGS_NONE );
                    GetSession()->bout << "\r\n|#1Edit any Text File: \r\n\n|#2Filename: ";
                    char szFileName[ MAX_PATH ];
                    _getcwd(szFileName, MAX_PATH);
                    snprintf( szFileName, sizeof( szFileName ), "%c", WWIV_FILE_SEPERATOR_CHAR );
                    std::string newFileName;
                    Input1( newFileName, szFileName, 50, true, INPUT_MODE_FILE_UPPER );
                    if ( !newFileName.empty() )
					{
                        external_edit( newFileName.c_str(), "", GetSession()->GetCurrentUser()->GetDefaultEditor() - 1, 500, ".", szFileName, MSGED_FLAG_NO_TAGLINE );
					}
                    holdphone( false );
                }
                break;
                // Print Network Pending list
            case 'P':
                if ( AllowLocalSysop() )
				{
                    wfc_cls();
                    print_pending_list();
				}
                break;
                // Quit BBS
            case 'Q':
                QuitBBS();
                break;
                // Read All Mail
            case 'R':
                wfc_cls();
                if ( AllowLocalSysop() )
                {
                    write_inst( INST_LOC_MAILR, 0, INST_FLAGS_NONE );
                    holdphone( true );
                    mailr();
                    holdphone( false );
                }
                break;
                // Print Current Status
            case 'S':
                if ( AllowLocalSysop() )
                {
                    prstatus( true );
                    getkey();
                }
                break;
                // Run Terminal Program
            case 'T':
#ifdef _DEBUG
                RunUnitTests("");
                getkey();
#else
                if ( AllowLocalSysop() && syscfg.terminal[0] )
                {
                    write_inst( INST_LOC_TERM, 0, INST_FLAGS_NONE );
                    ExecuteExternalProgram( syscfg.terminal, EFLAG_NONE );
                    imodem( true );
                    imodem( false );
                }
				else
				{
					GetSession()->localIO()->LocalGotoXY( 2, 23 );
					GetSession()->bout << "|#6No terminal program defined.";
				}
#endif // _DEBUG
                break;
                // UserEdit
            case 'U':
                if ( AllowLocalSysop() )
                {
                    write_inst( INST_LOC_UEDIT, 0, INST_FLAGS_NONE );
                    holdphone( true );
                    uedit( 1, UEDIT_NONE );
                    holdphone( false );
                }
                break;
                // Send Internet Mail
            case 'V':
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    GetSession()->usernum=1;
                    GetSession()->SetUserOnline( true );
                    holdphone( true );
                    get_user_ppp_addr();
                    send_inet_email();
                    GetSession()->SetUserOnline( false );
                    GetSession()->WriteCurrentUser( 1 );
                    cleanup_net();
                    holdphone( false );
                }
                break;
                // Edit Gfile
            case 'W':
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    write_inst( INST_LOC_TEDIT, 0, INST_FLAGS_NONE );
                    holdphone( true );
                    GetSession()->bout << "|#1Edit " << syscfg.gfilesdir << "<filename>: \r\n";
                    text_edit();
                    holdphone( false );
                }
                break;
                // Print Environment
            case 'X':
                break;
                // Print Yesterday's Log
            case 'Y':
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    WStatus *pStatus = GetStatusManager()->GetStatus();
                    char szSysopLogFileName[ MAX_PATH ];
                    GetSysopLogFileName( date(), szSysopLogFileName );
                    print_local_file( pStatus->GetLogFileName(), szSysopLogFileName );
                    delete pStatus;
                }
                break;
                // Print Activity (Z) Log
            case 'Z':
                if ( AllowLocalSysop() )
                {
                    zlog();
                    GetSession()->bout.NewLine();
                    getkey();
                }
                break;
            }
            wfc_cls();  // moved from after getch
            if ( !incom && !lokb )
            {
                frequent_init();
                GetSession()->ReadCurrentUser( 1 );
                read_qscn( 1, qsc, false );
                fwaiting = GetSession()->GetCurrentUser()->GetNumMailWaiting();
                GetSession()->ResetEffectiveSl();
                GetSession()->usernum = 1;
            }
            catsl();
            write_inst( INST_LOC_WFC, 0, INST_FLAGS_NONE );
        }
#if !defined ( __unix__ )
        if ( ok_modem_stuff && sess->remoteIO()->incoming() && !lokb )
        {
            any = true;
            if ( rpeek_wfconly() == SOFTRETURN )
            {
                bgetchraw();
            }
            else
            {
                if ( mode_switch( 1.0, false ) == mode_ring )
                {
                    if ( GetSession()->wfc_status == 1 )
                    {
                        GetSession()->localIO()->LocalXYAPrintf( 58, 13, 14, "%-20s", "Ringing...." );
                    }
                    answer_phone();
                }
                else if ( modem_mode == mode_con )
                {
                    incom = outcom = true;
                    if ( !( modem_flag & flag_ec ) )
                    {
                        wait1( 45 );
                    }
                    else
                    {
                        wait1( 2 );
                    }
                }
            }
        }
#endif // !__unix__
        if ( !any )
        {
            if ( GetSession()->GetMessageAreaCacheNumber() < GetSession()->num_subs )
            {
                if ( !GetSession()->m_SubDateCache[GetSession()->GetMessageAreaCacheNumber()] )
                {
                    any = true;
                    iscan1( GetSession()->GetMessageAreaCacheNumber(), true );
                }
                GetSession()->SetMessageAreaCacheNumber( GetSession()->GetMessageAreaCacheNumber() + 1);
            }
            else
            {
                if ( GetSession()->GetFileAreaCacheNumber() < GetSession()->num_dirs )
                {
                    if ( !GetSession()->m_DirectoryDateCache[GetSession()->GetFileAreaCacheNumber()] )
                    {
                        any = true;
                        dliscan_hash( GetSession()->GetFileAreaCacheNumber() );
                    }
                    GetSession()->SetFileAreaCacheNumber( GetSession()->GetFileAreaCacheNumber() + 1 );
                }
                else
                {
                    if ( this->IsCleanNetNeeded() || labs( timer1() - mult_time ) > 1000L )
                    {
                        cleanup_net();
                        mult_time = timer1();
                        giveup_timeslice();
                    }
                    else
                    {
                        giveup_timeslice();
                    }
                }
            }
        }
    } while ( !incom && !lokb );
	return lokb;
}


int WApplication::LocalLogon()
{
    GetSession()->localIO()->LocalGotoXY( 2, 23 );
    GetSession()->bout << "|#9Log on to the BBS?";
    double d = timer();
    int lokb = 0;
    while ( !GetSession()->localIO()->LocalKeyPressed() && ( fabs( timer() - d ) < SECONDS_PER_MINUTE_FLOAT ) )
        ;

    if ( GetSession()->localIO()->LocalKeyPressed() )
    {
        char ch = wwiv::UpperCase<char>( GetSession()->localIO()->getchd1() );
        if ( ch == 'Y' )
        {
            GetSession()->localIO()->LocalFastPuts( YesNoString( true ) );
            GetSession()->bout << wwiv::endl;
            lokb = 1;
            if ( ( syscfg.sysconfig & sysconfig_off_hook ) == 0 )
            {
                sess->remoteIO()->dtr( false );
            }
        }
        else if ( ch == 0 || static_cast<unsigned char>( ch ) == 224 )
        {
            // The ch == 224 is a Win32'ism
            GetSession()->localIO()->getchd1();
        }
        else
        {
            bool fast = false;
            if ( !AllowLocalSysop() )
            {
                return lokb;
            }

            if ( ch == 'F' ) // 'F' for Fast
            {
                m_unx = 1;
                fast = true;
            }
            else
            {
                switch ( ch )
                {
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    fast = true;
                    m_unx = ch - '0';
                    break;
                }
            }
            if ( !fast || m_unx > GetStatusManager()->GetUserCount() )
            {
                return lokb;
            }

            WUser tu;
            GetUserManager()->ReadUserNoCache( &tu, m_unx );
            if ( tu.GetSl() != 255 || tu.IsUserDeleted() )
            {
                return lokb;
            }

            GetSession()->usernum = m_unx;
            int nSavedWFCStatus = GetWfcStatus();
            SetWfcStatus( 0 );
            GetSession()->ReadCurrentUser();
            read_qscn( GetSession()->usernum, qsc, false );
            SetWfcStatus( nSavedWFCStatus );
            bputch( ch );
            GetSession()->localIO()->LocalPuts( "\r\n\r\n\r\n\r\n\r\n\r\n" );
            lokb = 2;
            if ( ( syscfg.sysconfig & sysconfig_off_hook ) == 0 )
            {
                sess->remoteIO()->dtr( false );
            }
            GetSession()->ResetEffectiveSl();
            changedsl();
            if ( !set_language( GetSession()->GetCurrentUser()->GetLanguage() ) )
            {
                GetSession()->GetCurrentUser()->SetLanguage( 0 );
                set_language( 0 );
            }
            return lokb;
        }
        if ( ch == 0 || static_cast<unsigned char>( ch ) == 224 )
        {
            // The 224 is a Win32'ism
            GetSession()->localIO()->getchd1();
        }
    }
    if ( lokb == 0 )
    {
        GetSession()->localIO()->LocalCls();
    }
    return lokb;
}


void WApplication::GotCaller( unsigned int ms, unsigned long cs )
{
    frequent_init();
    if ( GetSession()->wfc_status == 0 )
    {
        GetSession()->localIO()->LocalCls();
    }
    com_speed   = cs;
    modem_speed = static_cast<unsigned short>( ms );
    GetSession()->ReadCurrentUser( 1 );
    read_qscn( 1, qsc, false );
    GetSession()->ResetEffectiveSl();
    GetSession()->usernum = 1;
    if ( GetSession()->GetCurrentUser()->IsUserDeleted() )
    {
        GetSession()->GetCurrentUser()->SetScreenChars( 80 );
        GetSession()->GetCurrentUser()->SetScreenLines( 25 );
    }
    GetSession()->screenlinest = 25;
    GetSession()->localIO()->LocalCls();
    GetSession()->localIO()->LocalPrintf( "Logging on at %s...\r\n", GetSession()->GetCurrentSpeed().c_str() );
    if ( ms )
    {
        if ( ok_modem_stuff && NULL != sess->remoteIO() )
        {
            sess->remoteIO()->setup( 'N', 8, 1, cs );
        }
        incom   = true;
        outcom  = true;
        GetSession()->using_modem = 1;
    }
    else
    {
        GetSession()->using_modem = 0;
        incom   = false;
        outcom  = false;
    }
}


int WApplication::BBSMainLoop(int argc, char *argv[])
{
//
// TODO - move this to WIOTelnet
//
#if defined ( _WIN32 )
    WIOTelnet::InitializeWinsock();

    //
    // If there is only 1 argument "-TELSRV" then use internal telnet daemon
    //
    if ( argc == 2 )
    {
        if ( wwiv::stringUtils::IsEqualsIgnoreCase( argv[1], "-TELSRV" ) ||
             wwiv::stringUtils::IsEqualsIgnoreCase( argv[1], "/TELSRV" ) )
        {
            WInternalTelnetServer server( this );
            server.RunTelnetServer();
            ExitBBSImpl( 0 );
            return 0;
        }
    }
#endif // _WIN32

    // We are not running in the telnet server, so proceed as planned.
    int nReturnCode = Run(argc, argv);
    ExitBBSImpl( nReturnCode );
    return nReturnCode;
}


int WApplication::Run(int argc, char *argv[])
{
    int num_min                 = 0;
    unsigned int ui             = 0;
    unsigned long us            = 0;
    unsigned short this_usernum = 0;
    bool ooneuser               = false;
    bool event_only             = false;
    bool bTelnetInstance        = false;
    unsigned int hSockOrComm    = 0;

    char* ss = getenv("BBS");
    if ( ss )
    {
        if ( strncmp( ss, "WWIV", 4 ) == 0 )
        {
            std::cout << "You are already in the BBS, type 'EXIT' instead.\n\n";
            exit( 255 );
        }
    }
    curatr = 0x07;
    ss = getenv( "WWIV_DIR" );
    if ( ss )
    {
        WWIV_ChangeDirTo( ss );
    }

    // Set the instance, this may be changed by a command line argument
    m_nInstance = 1;
    no_hangup = false;
    ok_modem_stuff = true;
    GetSession()->SetGlobalDebugLevel( 0 );

#if defined( __unix__ )
    // HACK to make WWIV5/X just work w/o any command line
    m_bUserAlreadyOn = true;
    ui = us = 9600;
    ooneuser = true;
#endif

    std::string fullResultCode;
    std::string systemPassword;

    for ( int i = 1; i < argc; i++ )
    {
        std::string argumentRaw = argv[i];
        if ( argumentRaw.length() > 1 && ( argumentRaw[0] == '-' || argumentRaw[0] == '/' ) )
        {
            std::string argument = argumentRaw.substr( 2 );
			char ch = wwiv::UpperCase<char>( argumentRaw[1] );
            switch ( ch )
            {
            case 'B':
                {
                    ui = static_cast<unsigned int>( atol( argument.c_str() ) );
                    char szCurrentSpeed[ 21 ];
                    snprintf( szCurrentSpeed, sizeof( szCurrentSpeed ), "%u",  ui );
                    GetSession()->SetCurrentSpeed( szCurrentSpeed );
                    if (!us)
                    {
                        us = ui;
                    }
                    m_bUserAlreadyOn = true;
                }
                break;
            case 'C':
                break;
            case 'D':
                GetSession()->SetGlobalDebugLevel( atoi( argument.c_str() ) );
                break;
            case 'E':
                event_only = true;
                break;
            case 'F':
                fullResultCode = argument;
                StringUpperCase( fullResultCode );
                m_bUserAlreadyOn = true;
                break;
            case 'S':
                us = static_cast<unsigned int>( atol( argument.c_str() ) );
                if ( ( us % 300 ) && us != 115200 )
                {
                    us = ui;
                }
                break;
            case 'Q':
                m_nOkLevel = atoi( argument.c_str() );
                break;
            case 'A':
                m_nErrorLevel = atoi( argument.c_str() );
                break;
            case 'O':
                ooneuser = true;
                break;
            case 'H':
                hSockOrComm = atoi( argument.c_str() );
                break;
            case 'P':
                systemPassword = argument;
                StringUpperCase( systemPassword );
                break;
            case 'I':
            case 'N':
                {
                    m_nInstance = atoi( argument.c_str() );
                    if ( m_nInstance <= 0 || m_nInstance > 999 )
                    {
                        std::cout << "Your Instance can only be 1..999, you tried instance #" << m_nInstance << std::endl;
                        exit( m_nErrorLevel );
                    }
                }
                break;
            case 'M':
#if !defined ( __unix__ )
                ok_modem_stuff = false;
#endif
                break;
            case 'R':
                num_min = atoi( argument.c_str() );
                break;
            case 'U':
                this_usernum = wwiv::stringUtils::StringToUnsignedShort( argument.c_str() );
                if ( !m_bUserAlreadyOn )
                {
                    GetSession()->SetCurrentSpeed( "KB" );
                }
                m_bUserAlreadyOn = true;
                break;
/*
                case 'W':
                this->InitializeBBS();
				this->doWFCEvents();
				exit( m_nOkLevel );
*/
            case 'X':
                {
                    char argument2Char = wwiv::UpperCase<char>( argument.at(0) );
                    if ( argument2Char == 'T' || argument2Char == 'C' )
                    {
                        // This more of a hack to make sure the Telnet
                        // Server's -Bxxx parameter doesn't hose us.
                        GetSession()->SetCurrentSpeed( "115200" );
                        GetSession()->SetUserOnline( false );
                        us                  = 115200U;
                        ui                  = us;
                        m_bUserAlreadyOn    = true;
                        ooneuser            = true;
                        GetSession()->using_modem   = 0;
                        hangup              = false;
                        incom               = true;
                        outcom              = false;
                        global_xx           = false;
                        bTelnetInstance = ( argument2Char == 'T' ) ? true : false;
                    }
                    else
                    {
                        std::cout << "Invalid Command line argument given '" << argumentRaw << "'\r\n\n";
                        exit( m_nErrorLevel );
                    }
                }
                break;
            case 'Z':
                no_hangup = true;
                break;
                //
                // TODO Add handling for Socket and Comm handle here
                //
                //
			case 'K':
				{
					this->InitializeBBS();
					GetSession()->localIO()->LocalCls();
					if ( ( i + 1 ) < argc )
					{
						i++;
						GetSession()->bout << "\r\n|#7\xFE |#5Packing specified subs: \r\n";
						while ( i < argc )
						{
							int nSubNumToPack = atoi( argv[ i ] );
							pack_sub( nSubNumToPack );
							sysoplogf( "* Packed Message Area %d", nSubNumToPack );
							i++;
						}
					}
					else
					{
						GetSession()->bout << "\r\n|#7\xFE |#5Packing all subs: \r\n";
						sysoplog( "* Packing All Message Areas" );
						pack_all_subs( true );
					}
					ExitBBSImpl( m_nOkLevel );
				}
				break;
            case '?':
                ShowUsage();
                exit( 0 );
                break;
			case '-':
				{
                    if ( argumentRaw == "--help" )
					{
						ShowUsage();
						exit( 0 );
#ifdef _DEBUG
					} else if ( argumentRaw.substr(0, 6) == "--test" ) {
						this->InitializeBBS();
                        std::string suite = argumentRaw.length() > 7 ? argumentRaw.substr( 7 ) : "";
						bool ok = RunUnitTests( suite );
						ExitBBSImpl( ok ? m_nOkLevel : m_nErrorLevel );
#endif // _DEBUG
					}
				} break;
            default:
                {
                    std::cout << "Invalid Command line argument given '" << argument << "'\r\n\n";
                    exit( m_nErrorLevel );
                }
                break;
            }
        }
        else
        {
            // Command line argument did not start with a '-' or a '/'
            std::cout << "Invalid Command line argument given '" << argumentRaw << "'\r\n\n";
            exit( m_nErrorLevel );
        }
    }

    // Add the environment variable or overwrite the existing one
    char szInstanceEnvVar[81];
#if !defined ( __unix__ )
    snprintf( szInstanceEnvVar, sizeof( szInstanceEnvVar ), "WWIV_INSTANCE=%ld", GetInstanceNumber() );
    _putenv( szInstanceEnvVar );
#else
    // For some reason putenv() doesn't work sometimes when setenv() does...
    snprintf( szInstanceEnvVar, sizeof( szInstanceEnvVar ), "%u", GetInstanceNumber() );
    setenv( "WWIV_INSTANCE", szInstanceEnvVar, 1 );
    m_bUserAlreadyOn = true;
#endif

    GetSession()->CreateComm( bTelnetInstance, hSockOrComm );
    this->InitializeBBS();

    if ( systemPassword.length() > 0 )
    {
        strcpy( syscfg.systempw, systemPassword.c_str() );
    }

    if ( syscfg.sysconfig & sysconfig_no_local )
    {
        this_usernum = 0;
        m_bUserAlreadyOn = false;
    }
    if ( fullResultCode.length() > 0  )
    {
        process_full_result( fullResultCode );
    }

    GetSession()->localIO()->UpdateNativeTitleBar();

    // If we are telnet...
    if ( bTelnetInstance )
    {
	    // If the com port is set to 0 here, then ok_modem_stuff is cleared
	    // in the call to init.  Well.. If we are running native sockets, we
	    // could care less about the com port... So set it back to true here
	    // ... the better solution would just be to tell init to "get bent"
	    ok_modem_stuff = true;
        sess->remoteIO()->open();
    }

    if ( num_min > 0 )
    {
        syscfg.executetime = static_cast< unsigned short >(  ( timer() + num_min * 60 ) / 60 );
        if ( syscfg.executetime > 1440 )
        {
            syscfg.executetime -= 1440;
        }
        syscfg.executestr[0] = '\0';
        time_event = static_cast<double>( syscfg.executetime ) * MINUTES_PER_HOUR_FLOAT;
        last_time = time_event - timer();
        if ( last_time < 0.0 )
        {
			last_time += HOURS_PER_DAY_FLOAT * SECONDS_PER_HOUR_FLOAT;
        }
    }

    if ( event_only )
    {
        std::auto_ptr<WStatus> pStatus(GetStatusManager()->GetStatus());
        cleanup_events();
        if ( !wwiv::stringUtils::IsEquals( date(), pStatus->GetLastDate() ) )
        {
            // This may be another node, but the user explicitly wanted to run the beginday
            // event from the commandline, so we'll just check the date.
            beginday( true );
        }
        else
        {
            sysoplog( "!!! Wanted to run the beginday event when it's not required!!!", false );
            std::cout << "! WARNING: Tried to run beginday event again\r\n\n";
            WWIV_Delay( 2000 );
        }
        ExitBBSImpl( m_nOkLevel );
    }

    do
    {
        if ( this_usernum )
        {
            GetSession()->usernum = this_usernum;
            GetSession()->ReadCurrentUser();
            if ( !GetSession()->GetCurrentUser()->IsUserDeleted() )
            {
                GotCaller( ui, us );
                GetSession()->usernum = this_usernum;
                GetSession()->ReadCurrentUser();
                read_qscn( GetSession()->usernum, qsc, false );
                GetSession()->ResetEffectiveSl();
                changedsl();
                okmacro = true;
                if ( !hangup && GetSession()->usernum > 0 &&
                     GetSession()->GetCurrentUser()->IsRestrictionLogon() &&
                     wwiv::stringUtils::IsEquals( date(), GetSession()->GetCurrentUser()->GetLastOn() ) &&
                     GetSession()->GetCurrentUser()->GetTimesOnToday() > 0 )
                {
                    GetSession()->bout << "\r\n|#6Sorry, you can only logon once per day.\r\n\n";
                    hangup = true;
                }
            }
            else
            {
                this_usernum = 0;
            }
        }
        if ( !this_usernum )
        {
            if ( m_bUserAlreadyOn )
            {
                GotCaller( ui, us );
            }
#if !defined ( __unix__ )
            else
            {
                GetCaller();
            }
#endif
        }

        if ( modem_mode == mode_fax )
        {
            if ( WFile::ExistsWildcard( "WWIVFAX.*" ) )
            {
				const std::string command = stuff_in( "WWIVFAX %S %P", "", "", "", "", "" );
                ExecuteExternalProgram( command, EFLAG_NONE );
            }
            goto hanging_up;
        }
        if ( GetSession()->using_modem > -1 )
        {
            if ( !GetSession()->using_modem )
            {
                holdphone( true );
            }
            if ( !this_usernum )
            {
                getuser();
            }
        }
        else
        {
            holdphone( true );
            GetSession()->using_modem = 0;
            okmacro = true;
            GetSession()->usernum = m_unx;
            GetSession()->ResetEffectiveSl();
            changedsl();
        }
        this_usernum = 0;
        if ( !hangup )
        {
            logon();
            setiia( 90 );
            set_net_num( 0 );
            while ( !hangup )
            {
                if ( filelist )
                {
                    BbsFreeMemory( filelist );
                    filelist = NULL;
                }
                zap_ed_info();
                write_inst( INST_LOC_MAIN, usub[GetSession()->GetCurrentMessageArea()].subnum, INST_FLAGS_NONE );
                mainmenu();
            }
            logoff();
        }
    hanging_up:

        if ( !no_hangup && GetSession()->using_modem && ok_modem_stuff )
        {
            hang_it_up();
        }
        catsl();
        frequent_init();
        if ( GetSession()->wfc_status == 0 )
        {
            GetSession()->localIO()->LocalCls();
        }
        cleanup_net();

        if ( !GetSession()->using_modem )
        {
            holdphone( false );
        }
        if ( !no_hangup && ok_modem_stuff )
        {
            sess->remoteIO()->dtr( false );
        }
        m_bUserAlreadyOn = false;
        if ( GetSession()->localIO()->GetSysopAlert() && (!GetSession()->localIO()->LocalKeyPressed() ) )
        {
            sess->remoteIO()->dtr( true );
            wait1( 2 );
            holdphone( true );
            double dt = timer();
            GetSession()->localIO()->LocalCls();
            GetSession()->bout << "\r\n>> SYSOP ALERT ACTIVATED <<\r\n\n";
            while ( !GetSession()->localIO()->LocalKeyPressed() && ( fabs( timer() - dt ) < SECONDS_PER_MINUTE_FLOAT ) )
            {
				WWIV_Sound( 500, 250 );
				WWIV_Delay( 1 );
            }
            GetSession()->localIO()->LocalCls();
            holdphone( false );
        }
        GetSession()->localIO()->SetSysopAlert( false );
    } while ( !ooneuser );

    return m_nOkLevel;
}



void WApplication::ShowUsage()
{
    std::cout << "WWIV Bulletin Board System [" << wwiv_version << " - " << beta_version << "]\r\n\n" <<
                "Usage:\r\n\n" <<
                "WWIV50 -I<inst> [options] \r\n\n" <<
                "Options:\r\n\n" <<
                "  -?         - Display command line options (This screen)\r\n\n" <<
                "  -A<level>  - Specify the Error Exit Level\r\n" <<
                "  -B<rate>   - Someone already logged on at rate (modem speed)\r\n" <<
                "  -E         - Load for beginday event only\r\n" <<
                "  -F         - Pass full result code (\"CONNECT 33600/ARQ/HST/V.42BIS\")\r\n" <<
                "  -H<handle> - Socket or Comm handle\r\n" <<
                "  -I<inst>   - Designate instance number <inst>\r\n" <<
	            "  -K [# # #] - Pack Message Areas, optionally list the area(s) to pack\r\n" <<
                "  -M         - Don't access modem at all\r\n" <<
                "  -N<inst>   - Designate instance number <inst>\r\n" <<
                "  -O         - Quit WWIV after one user done\r\n" <<
                "  -P<pass>   - Set System Password to <pass>\r\n" <<
                "  -Q<level>  - Normal exit level\r\n" <<
                "  -R<min>    - Specify max # minutes until event\r\n" <<
                "  -S<rate>   - Used only with -B, indicates com port speed\r\n" <<
#if defined (_WIN32)
                "  -TELSRV    - Uses internet telnet server to answer incomming session\r\n" <<
#endif // _WIN32
                "  -U<user#>  - Pass usernumber <user#> online\r\n" <<
#if defined (_WIN32)
                "  -XT        - Someone already logged on via telnet (socket handle)\r\n" <<
#endif // _WIN32
                "  -XC        - Someone already logged on via modem (comm handle)\r\n" <<
                "  -Z         - Do not hang up on user when at log off\r\n" <<
    std::endl;
}



WApplication::WApplication()
{
    sess			        = new WSession( this );
    statusMgr			    = new StatusMgr();
    userManager			    = new WUserManager();
    m_nOkLevel			    = WApplication::exitLevelOK;
    m_nErrorLevel		    = WApplication::exitLevelNotOK;
    m_nInstance			    = 1;
	m_bUserAlreadyOn	    = false;
    m_nBbsShutdownStatus    = WApplication::shutdownNone;
    m_fShutDownTime         = 0.0;
    m_nWfcStatus = 0;

    WFile::SetLogger( this );
    WFile::SetDebugLevel( GetSession()->GetGlobalDebugLevel() );

    // TODO this should move into the WSystemConfig object (syscfg wrapper) once it is established.
    if(syscfg.userreclen == 0)
    {
        syscfg.userreclen = sizeof(userrec);
    }

    _tzset();

	// Set the home directory
	_getcwd( m_szCurrentDirectory, MAX_PATH );

}


WApplication::WApplication( const WApplication& copy )
{
    statusMgr = copy.statusMgr;
    userManager = copy.userManager;
    m_nOkLevel = copy.m_nOkLevel;
    m_nErrorLevel = copy.m_nErrorLevel;
    m_nInstance = copy.m_nInstance;
    m_bUserAlreadyOn = copy.m_bUserAlreadyOn;
    m_fShutDownTime = copy.m_fShutDownTime;
    m_nWfcStatus = copy.m_nWfcStatus;
    
    strcpy( m_szCurrentDirectory, copy.m_szCurrentDirectory );
}


void WApplication::CdHome()
{
	WWIV_ChangeDirTo( m_szCurrentDirectory );
}


const std::string WApplication::GetHomeDir()
{
	std::string dir = m_szCurrentDirectory;
	dir += WWIV_FILE_SEPERATOR_CHAR;
	return std::string( dir );
}

void WApplication::AbortBBS( bool bSkipShutdown )
{
    if ( bSkipShutdown )
    {
        exit( m_nErrorLevel );
    }
    else
    {
        ExitBBSImpl( m_nErrorLevel );
    }
}


void WApplication::QuitBBS()
{
    ExitBBSImpl( WApplication::exitLevelQuit );
}


void WApplication::ExitBBSImpl( int nExitLevel )
{
    sysoplog( "", false );
    sysoplogfi( false, "WWIV %s, inst %u, taken down at %s on %s with exit code %d.",
			  wwiv_version, GetInstanceNumber(), times(), fulldate(), nExitLevel );
    sysoplog( "", false );
    catsl();
    close_strfiles();
    write_inst( INST_LOC_DOWN, 0, INST_FLAGS_NONE );
    std::cout << "WWIV Bulletin Board System " << wwiv_version << beta_version << " exiting at error level " << nExitLevel << std::endl << std::endl;
    delete this;
    exit( nExitLevel );

}


bool WApplication::LogMessage( const char* pszFormat, ... )
{
    va_list ap;
    char szBuffer[2048];

    va_start( ap, pszFormat );
    WWIV_VSNPRINTF( szBuffer, sizeof( szBuffer ), pszFormat, ap );
    va_end( ap );
    sysoplog( szBuffer );
    return true;
}


void WApplication::UpdateTopScreen()
{
    if (!GetWfcStatus()) 
    {
        WStatus* pStatus = GetStatusManager()->GetStatus();
        GetSession()->localIO()->UpdateTopScreen( pStatus, GetSession(), GetInstanceNumber() ); 
        delete pStatus;
    }
}


void WApplication::ShutDownBBS( int nShutDownStatus )
{
    char xl[81], cl[81], atr[81], cc;
    GetSession()->localIO()->SaveCurrentLine( cl, atr, xl, &cc );

    switch ( nShutDownStatus )
	{
    case 1:
        SetShutDownTime( timer() + 180.0 );
    case 2:
    case 3:
        SetShutDownStatus( nShutDownStatus );
        GetSession()->bout.NewLine( 2 );
        GetSession()->bout << "|#7***\r\n|#7To All Users, System will shut down in " <<
                                4 - GetShutDownStatus() << " minunte(s) for maintenance.\r \n" <<
                                "|#7Please finish your session and log off. Thank you\r\n|#7***\r\n";
        break;
    case 4:
        GetSession()->bout.NewLine( 2 );
        GetSession()->bout << "|#7***\r\n|#7Please call back later.\r\n|#7***\r\n\n";
        GetSession()->GetCurrentUser()->SetExtraTime( GetSession()->GetCurrentUser()->GetExtraTime() + static_cast<float>( nsl() ) );
		GetSession()->bout << "Time on   = " << ctim( timer() - timeon ) << wwiv::endl;
        printfile( LOGOFF_NOEXT );
        hangup = true;
        SetShutDownStatus( WApplication::shutdownNone );
        break;
	default:
        std::cout << "[utility.cpp] shutdown called with illegal type: " << nShutDownStatus << std::endl;
		WWIV_ASSERT( false );
    }
    RestoreCurrentLine( cl, atr, xl, &cc );
}


void WApplication::UpdateShutDownStatus()
{
    if ( IsShutDownActive() )
    {
        if ((( GetShutDownTime() - timer()) < 120) && ((GetShutDownTime() - timer()) > 60))
        {
            if ( GetShutDownStatus() != WApplication::shutdownTwoMinutes )
            {
                ShutDownBBS( WApplication::shutdownTwoMinutes );
            }
        }
        if (((GetShutDownTime() - timer()) < 60) && ((GetShutDownTime() - timer()) > 0))
        {
            if ( GetShutDownStatus() != WApplication::shutdownOneMinute )
            {
                ShutDownBBS( WApplication::shutdownOneMinute );
            }
        }
        if ( ( GetShutDownTime() - timer() ) <= 0 )
        {
            ShutDownBBS( WApplication::shutdownImmediate );
        }
    }
}


void WApplication::ToggleShutDown()
{
    if ( IsShutDownActive() )
    {
        SetShutDownStatus( WApplication::shutdownNone );
    }
    else
    {
        ShutDownBBS( WApplication::shutdownThreeMinutes );
    }
}


WApplication::~WApplication()
{
    if ( sess != NULL )
    {
        delete sess;
        sess = NULL;
    }

	if ( statusMgr != NULL )
	{
		delete statusMgr;
		statusMgr = NULL;
	}

    if ( userManager != NULL )
    {
        delete userManager;
        userManager = NULL;
    }
}


int main( int argc, char *argv[] )
{
    app = new WApplication();
    return GetApplication()->BBSMainLoop( argc, argv );
}
