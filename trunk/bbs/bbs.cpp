// $Header$
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

#define _DEFINE_GLOBALS_ 
#include "wwiv.h"
#include "WStringUtils.h"
#include <sstream>
#undef _DEFINE_GLOBALS_

#include "bbs.h"

extern char cid_num[], cid_name[];
static bool bUsingPppProject = true;
extern int last_time_c;


//////////////////////////////////////////////////////////////////////////////
//
// Implementation
//
//
//


const int WBbsApp::exitLevelOK          = 0;
const int WBbsApp::exitLevelNotOK       = 1;
const int WBbsApp::exitLevelQuit        = 2;

const int WBbsApp::shutdownNone         = 0;
const int WBbsApp::shutdownThreeMinutes = 1;
const int WBbsApp::shutdownTwoMinutes   = 2;
const int WBbsApp::shutdownOneMinute    = 3;
const int WBbsApp::shutdownImmediate    = 4;


WBbsApp* GetApplication() 
{
    return app;
}


WLocalIO* WBbsApp::GetLocalIO() 
{ 
    return localIO; 
}


WComm* WBbsApp::GetComm()
{
    return comm;
}


StatusMgr* WBbsApp::GetStatusManager()
{
    return statusMgr;
}


WUserManager* WBbsApp::GetUserManager()
{
    return userManager;
}


#ifndef _UNIX
void WBbsApp::GetCaller()
{
    sess->SetMessageAreaCacheNumber( 0 );
    sess->SetFileAreaCacheNumber( 0 );
    SetShutDownStatus( WBbsApp::shutdownNone );
    wfc_init();
    cid_num[0] = 0;
    cid_name[0] = 0;
    frequent_init();
    if (sess->wfc_status == 0)
    {
        GetLocalIO()->LocalCls();
    }
    imodem( false );
    sess->usernum = 0;
    GetLocalIO()->SetWfcStatus( 0 );
    write_inst( INST_LOC_WFC, 0, INST_FLAGS_NONE );
    sess->ReadCurrentUser( 1 );
    read_qscn( 1, qsc, false );
    sess->usernum = 1;
    sess->ResetEffectiveSl();
    fwaiting = sess->thisuser.GetNumMailWaiting();
    if ( sess->thisuser.isUserDeleted() )
    {
        sess->thisuser.SetScreenChars( 80 );
        sess->thisuser.SetScreenLines( 25 );
    }
    sess->screenlinest = defscreenbottom + 1;

    int lokb = doWFCEvents();

    If ( lokb )
    {
        if ( ok_modem_stuff )
        {
            modem_speed = modem_i->defl.modem_speed;
        }
        else
        {
            modem_speed = 14400;
        }
    }

    sess->using_modem = incom;
    if ( lokb == 2 )
    {
        sess->using_modem = -1;
    }

    okskey = true;
    GetLocalIO()->LocalCls();
    GetLocalIO()->LocalPrintf( "%s %s ...\r\n",
                                ( ( modem_mode == mode_fax ) ? "Fax connection at" : "Logging on at" ),
                                sess->GetCurrentSpeed().c_str() );
    GetLocalIO()->SetWfcStatus( 0 );
}
#else

void wfc_screen() {}
void wfc_cls() {}
#endif


int WBbsApp::doWFCEvents()
{
    char ch;
    int lokb;
    static int mult_time;

    do
    {
        write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
        set_net_num( 0 );
        bool any = false;
        GetLocalIO()->SetWfcStatus( 1 );
        if ( !wwiv::stringUtils::IsEquals( date(), status.date1 ) )
        {
            if ( ( sess->GetBeginDayNodeNumber() == 0 ) || ( m_nInstance == sess->GetBeginDayNodeNumber() ) )
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
        sess->SetCurrentSpeed( "KB" );
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
        if (GetLocalIO()->LocalKeyPressed())
        {
            GetLocalIO()->SetWfcStatus( 0 );
            sess->ReadCurrentUser( 1 );
            read_qscn(1, qsc, false);
            fwaiting = sess->thisuser.GetNumMailWaiting();
            GetLocalIO()->SetWfcStatus( 1 );
            ch = wwiv::UpperCase<char>( GetLocalIO()->getchd1() );
            if (!ch)
            {
                ch = GetLocalIO()->getchd1();
                GetLocalIO()->skey(ch);
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
            GetLocalIO()->SetWfcStatus( 2 );
            any = true;
            okskey = true;
            resetnsp();
            GetLocalIO()->SetCursor( WLocalIO::cursorNormal );
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
                        GetLocalIO()->LocalCls();
                        nl();
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
                if ( net_sysnum > 0 || sess->GetMaxNetworkNumber() > 1 && AllowLocalSysop() )
                {
                    GetLocalIO()->LocalGotoXY( 2, 23 );
                    sess->bout << "|#9(|#2Q|#9=|#2Quit|#9) Display Which NETDAT Log File (|#10|#9-|#12|#9): ";
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
		        GetLocalIO()->LocalGotoXY( 2, 23 );
                sess->bout << "|#7Exit the BBS? ";
                if ( yesno() )
                {
                    QuitBBS();
                }
                GetLocalIO()->LocalCls();
                break;
                // Answer Phone
            case 'A':
                if ( !ok_modem_stuff )
                {
                    break;
                }
				GetLocalIO()->LocalGotoXY( 2, 23 );
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
                    sess->usernum = 1;
                    holdphone( true );
                    sess->bout << "|#1Send Email:";
                    send_email();
                    sess->WriteCurrentUser( 1 );
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
                    sess->usernum = 1;
                    holdphone( true );
                    sess->bout << "|#1Send any Text File in Email:\r\n\n|#2Filename: ";
                    std::string buffer;
                    input( buffer, 50 );
                    LoadFileIntoWorkspace( buffer.c_str(), false );
                    send_email();
                    sess->WriteCurrentUser( 1 );
                    cleanup_net();
                    holdphone( false );
                }
                break;
                // Print Log Daily logs
            case 'L':
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    GetStatusManager()->Read();
                    char szBuffer[ 255 ];
                    slname( date(), szBuffer );
                    print_local_file( szBuffer, status.log1 );
                }
                break;
                // Read User Mail
            case 'M':
                if ( AllowLocalSysop() )
                {
                    wfc_cls();
                    sess->usernum = 1;
                    holdphone( true );
                    readmail( 0 );
                    sess->WriteCurrentUser( 1 );
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
                    sess->bout << "\r\n|#1Edit any Text File: \r\n\n|#2Filename: ";
                    char szFileName[ MAX_PATH ];
                    getcwd(szFileName, MAX_PATH);
                    snprintf( szFileName, sizeof( szFileName ), "%c", WWIV_FILE_SEPERATOR_CHAR );
                    std::string newFileName;
                    Input1( newFileName, szFileName, 50, true, UPPER );
                    if ( !newFileName.empty() )
					{
                        external_edit( newFileName.c_str(), "", sess->thisuser.GetDefaultEditor() - 1, 500, ".", szFileName, MSGED_FLAG_NO_TAGLINE );
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
                if ( AllowLocalSysop() && syscfg.terminal[0] )
                {
                    write_inst( INST_LOC_TERM, 0, INST_FLAGS_NONE );
                    ExecuteExternalProgram( syscfg.terminal, EFLAG_NONE );
                    imodem( true );
                    imodem( false );
                }
				else
				{
					GetLocalIO()->LocalGotoXY( 2, 23 );
					sess->bout << "|12No terminal program defined.";
				}
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
                    sess->usernum=1;
                    sess->SetUserOnline( true );
                    holdphone( true );
                    get_user_ppp_addr();
                    send_inet_email();
                    sess->SetUserOnline( false );
                    sess->WriteCurrentUser( 1 );
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
                    sess->bout << "|#1Edit " << syscfg.gfilesdir << "<filename>: \r\n";
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
                    GetStatusManager()->Read();
                    char szDate[ 255 ];
                    slname( date(), szDate );
                    print_local_file( status.log1, szDate );
                }
                break;
                // Print Activity (Z) Log
            case 'Z':
                if ( AllowLocalSysop() )
                {
                    zlog();
                    nl();
                    getkey();
                }
                break;
            }
            wfc_cls();  // moved from after getch
            if ( !incom && !lokb )
            {
                frequent_init();
                sess->ReadCurrentUser( 1 );
                read_qscn( 1, qsc, false );
                fwaiting = sess->thisuser.GetNumMailWaiting();
                sess->ResetEffectiveSl();
                sess->usernum = 1;
            }
            catsl();
            write_inst( INST_LOC_WFC, 0, INST_FLAGS_NONE );
        }
#ifndef _UNIX
        if ( ok_modem_stuff && comm->incoming() && !lokb )
        {
            any = true;
            if ( rpeek_wfconly() == SOFTRETURN )
            {
                bgetchraw();
            }
            else
            {
                if ( mode_switch( 1.0, 0 ) == mode_ring )
                {
                    if ( sess->wfc_status == 1 )
                    {
                        GetLocalIO()->LocalXYAPrintf( 58, 13, 14, "%-20s", "Ringing...." );
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
#endif // !_UNIX
        if ( !any )
        {
            if ( sess->GetMessageAreaCacheNumber() < sess->num_subs )
            {
                if ( !sess->m_SubDateCache[sess->GetMessageAreaCacheNumber()] )
                {
                    any = true;
                    iscan1( sess->GetMessageAreaCacheNumber(), true );
                }
                sess->SetMessageAreaCacheNumber( sess->GetMessageAreaCacheNumber() + 1);
            }
            else
            {
                if ( sess->GetFileAreaCacheNumber() < sess->num_dirs )
                {
                    if ( !sess->m_DirectoryDateCache[sess->GetFileAreaCacheNumber()] )
                    {
                        any = true;
                        dliscan_hash( sess->GetFileAreaCacheNumber() );
                    }
                    sess->SetFileAreaCacheNumber( sess->GetFileAreaCacheNumber() + 1 );
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


int WBbsApp::LocalLogon()
{
    GetLocalIO()->LocalGotoXY( 2, 23 );
    sess->bout << "|#9Log on to the BBS?";
    double d = timer();
    int lokb = 0;
    while ( !GetLocalIO()->LocalKeyPressed() && ( fabs( timer() - d ) < SECONDS_PER_MINUTE_FLOAT ) )
        ;

    if ( GetLocalIO()->LocalKeyPressed() )
    {
        char ch = wwiv::UpperCase<char>( GetLocalIO()->getchd1() );
        if ( ch == 'Y' )
        {
            GetLocalIO()->LocalFastPuts( YesNoString( true ) );
            sess->bout << wwiv::endl;
            lokb = 1;
            if ( ( syscfg.sysconfig & sysconfig_off_hook ) == 0 )
            {
                comm->dtr( false );
            }
        }
        else if ( ch == 0 || static_cast<unsigned char>( ch ) == 224 )
        {
            // The ch == 224 is a Win32'ism
            GetLocalIO()->getchd1();
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
            GetStatusManager()->Read();
            if ( !fast || ( m_unx > status.users ) )
            {
                return lokb;
            }

            WUser tu;
            GetUserManager()->ReadUserNoCache( &tu, m_unx );
            if ( tu.GetSl() != 255 || tu.isUserDeleted() )
            {
                return lokb;
            }

            sess->usernum = m_unx;
            int nSavedWFCStatus = GetLocalIO()->GetWfcStatus();
            GetLocalIO()->SetWfcStatus( 0 );
            sess->ReadCurrentUser( sess->usernum );
            read_qscn( sess->usernum, qsc, false );
            GetLocalIO()->SetWfcStatus( nSavedWFCStatus );
            bputch( ch );
            GetLocalIO()->LocalPuts( "\r\n\r\n\r\n\r\n\r\n\r\n" );
            lokb = 2;
            if ( ( syscfg.sysconfig & sysconfig_off_hook ) == 0 )
            {
                comm->dtr( false );
            }
            sess->ResetEffectiveSl();
            changedsl();
            if ( !set_language( sess->thisuser.GetLanguage() ) )
            {
                sess->thisuser.SetLanguage( 0 );
                set_language( 0 );
            }
            return lokb;
        }
        if ( ch == 0 || static_cast<unsigned char>( ch ) == 224 )
        {
            // The 224 is a Win32'ism
            GetLocalIO()->getchd1();
        }
    }
    if ( lokb == 0 )
    {
        GetLocalIO()->LocalCls();
    }
    return lokb;
}


void WBbsApp::GotCaller( unsigned int ms, unsigned long cs )
{
    frequent_init();
    if ( sess->wfc_status == 0 )
    {
        GetLocalIO()->LocalCls();
    }
    com_speed   = cs;
    modem_speed = static_cast<unsigned short>( ms );
    sess->ReadCurrentUser( 1 );
    read_qscn( 1, qsc, false );
    sess->ResetEffectiveSl();
    sess->usernum = 1;
    if ( sess->thisuser.isUserDeleted() )
    {
        sess->thisuser.SetScreenChars( 80 );
        sess->thisuser.SetScreenLines( 25 );
    }
    sess->screenlinest = 25;
    GetLocalIO()->LocalCls();
    GetLocalIO()->LocalPrintf( "Logging on at %s...\r\n", sess->GetCurrentSpeed().c_str() );
    if ( ms )
    {
        if ( ok_modem_stuff && NULL != comm )
        {
            comm->setup( 'N', 8, 1, cs );
        }
        incom   = true;
        outcom  = true;
        sess->using_modem = 1;
    }
    else
    {
        sess->using_modem = 0;
        incom   = false;
        outcom  = false;
    }
}


#if defined (_WIN32)

SOCKET hSocketHandle;


void CreateListener()
{
    int nRet = SOCKET_ERROR;
    SOCKET hSock;
    SOCKADDR_IN pstSockName;

    if ( sess->hSocket != INVALID_SOCKET )
    {
        // close all allocated resources
        shutdown( sess->hSocket, 2 );
        closesocket( sess->hSocket );
        sess->hSocket = INVALID_SOCKET;
    }
    // Start Listening Thread Socket
    hSock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( hSock == INVALID_SOCKET )
    {
        std::cout << "Error Creating Listener socket..\r\n";
    }
    else
    {
        pstSockName.sin_addr.s_addr = ADDR_ANY;
        pstSockName.sin_family = PF_INET;
        pstSockName.sin_port = htons( 23 );
        nRet = bind( hSock, reinterpret_cast<LPSOCKADDR>( &pstSockName ), sizeof( SOCKADDR_IN ) );
        if ( nRet == SOCKET_ERROR )
        {
			int nBindErrCode = WSAGetLastError();
            std::cout << "error " << nBindErrCode << " binding socket\r\n";
			switch ( nBindErrCode )
			{
			case WSANOTINITIALISED:
                std::cout << "WSANOTINITIALISED";
				break;
			case WSAENETDOWN:
				std::cout << "WSAENETDOWN";
				break;
			case WSAEACCES:
				std::cout << "WSAEACCES";
				break;
			case WSAEADDRINUSE:
				std::cout << "WSAEADDRINUSE";
				break;
			case WSAEADDRNOTAVAIL:
				std::cout << "WSAEADDRNOTAVAIL";
				break;
			case WSAEFAULT:
				std::cout << "WSAEFAULT";
				break;
			case WSAEINPROGRESS:
				std::cout << "WSAEINPROGRESS";
				break;
			case WSAEINVAL:
				std::cout << "WSAEINVAL";
				break;
			case WSAENOBUFS:
				std::cout << "WSAENOBUFS";
				break;
			case WSAENOTSOCK:
				std::cout << "WSAENOTSOCK";
				break;
			default:
				std::cout << "*unknown error*";
				break;
			}
        }
        else
        {
            nRet = listen( hSock, 5 );
            if ( nRet == SOCKET_ERROR )
            {
                std::cout << "Error listening on socket\r\n";
            }
        }
    }

    if ( nRet == SOCKET_ERROR )
    {
        closesocket( hSock );
        hSock = INVALID_SOCKET;
        std::cout << "Unable to initilize Listening Socket!\r\n";
        WSACleanup();
        exit( 1 );
    }
    hSocketHandle = hSock;
}


void WBbsApp::TelnetMainLoop()
{
    SOCKET hSock;
    SOCKADDR_IN lpstName;
    int AddrLen = sizeof( SOCKADDR_IN );

    if( hSocketHandle != INVALID_SOCKET )
    {
        std::cout << "Press control-c to exit\r\n\n";
        std::cout << "Waiting for socket connection...\r\n\n";
        hSock = accept( hSocketHandle, reinterpret_cast<LPSOCKADDR>( &lpstName ), &AddrLen );
        if( hSock != INVALID_SOCKET )
        {
            char buffer[20];
            snprintf( buffer, sizeof( buffer ), "-H%u", hSock );
            char **szParameters;
            szParameters = new char *[3];
            szParameters[0] = new char [1];
            szParameters[1] = new char [20];
            szParameters[2] = new char [20];

            strcpy( szParameters[0], "" );
            strcpy( szParameters[1], buffer );
            strcpy( szParameters[2], "-XT" );

            BBSmain( 3, szParameters );
            delete [] szParameters[0];
            delete [] szParameters[1];
            delete [] szParameters[2];
            delete [] szParameters;
        }
    }
}


#endif // _WIN32


int WBbsApp::Run(int argc, char *argv[])
{
//
// Only do the telnet listener on WIN32 platforms
//
#if defined ( _WIN32 )

    WSADATA wsaData;
    int err = WSAStartup( 0x0101, &wsaData );

	if ( err != 0 )
	{
		switch ( err )
		{
		case WSASYSNOTREADY:
			std::cout << "Error from WSAStartup: WSASYSNOTREADY";
			break;
		case WSAVERNOTSUPPORTED:
			std::cout << "Error from WSAStartup: WSAVERNOTSUPPORTED";
			break;
		case WSAEINPROGRESS:
			std::cout << "Error from WSAStartup: WSAEINPROGRESS";
			break;
		case WSAEPROCLIM:
			std::cout << "Error from WSAStartup: WSAEPROCLIM";
			break;
		case WSAEFAULT:
			std::cout << "Error from WSAStartup: WSAEFAULT";
			break;
		default:
			std::cout << "Error from WSAStartup: ** unknown error code **";
			break;
		}
	}

    //
    // If there is only 1 argument "-TELSRV" then use internal telnet daemon
    //
    if ( argc == 2 )
    {
        if ( wwiv::stringUtils::IsEqualsIgnoreCase( argv[1], "-TELSRV" ) ||
             wwiv::stringUtils::IsEqualsIgnoreCase( argv[1], "/TELSRV" ) )
        {
            CreateListener();
            TelnetMainLoop();
            ExitBBSImpl( 0 );
            return 0;
        }
    }
#endif // _WIN32

    // We are not running in the telnet server, so proceed as planned.
    int nReturnCode = BBSmain(argc, argv);
    ExitBBSImpl( nReturnCode );
    return nReturnCode;
}


int WBbsApp::BBSmain(int argc, char *argv[])
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
    m_nInstance                 = 1;
    no_hangup                   = false;
    ok_modem_stuff              = true;
    sess->SetGlobalDebugLevel( 0 );

#ifdef _UNIX
    // HACK to make WWIV5/X just work w/o any command line
    m_bUserAlreadyOn = true;
    ui = us = 9600;
    ooneuser = true;
#endif

    char szFullResultCode[ 81 ];
    szFullResultCode[0] = '\0';

    char szSystemPassword[ 81 ];
    szSystemPassword[0] = '\0';

    for ( int i = 1; i < argc; i++ )
    {
        char s[ 256 ];
        strcpy( s, argv[i] );
        if ( s[0] == '-' || s[0] == '/' )
        {
			char ch = wwiv::UpperCase<char>( s[1] );
            switch ( ch )
            {
            case 'B':
                {
                    ui = static_cast<unsigned int>( atol(&(s[2]) ));
                    char szCurrentSpeed[ 21 ];
                    snprintf( szCurrentSpeed, sizeof( szCurrentSpeed ), "%u",  ui );
                    sess->SetCurrentSpeed( szCurrentSpeed );
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
                sess->SetGlobalDebugLevel( atoi( &( s[2] ) ) );
                break;
            case 'E':
                event_only = true;
                break;
            case 'F':
                strcpy( szFullResultCode, s + 2 );
                strupr( szFullResultCode );
                m_bUserAlreadyOn = true;
                break;
            case 'S':
                us = static_cast<unsigned int>( atol( &( s[2] ) ) );
                if ( ( us % 300 ) && us != 115200 )
                {
                    us = ui;
                }
                break;
            case 'Q':
                m_nOkLevel = atoi(&(s[2]));
                break;
            case 'A':
                m_nErrorLevel = atoi(&(s[2]));
                break;
            case 'O':
                ooneuser = true;
                break;
            case 'H':
                hSockOrComm = atoi( &s[2] );
                break;
            case 'P':
                strcpy( szSystemPassword, s + 2 );
                strupr( szSystemPassword );
                break;
            case 'I':
            case 'N':
                m_nInstance = atoi( &( s[2] ) );
                if ( m_nInstance <= 0 || m_nInstance > 999 )
                {
                    std::cout << "Your Instance can only be 1..999, you tried instance #" << m_nInstance << std::endl;
                    exit( m_nErrorLevel );
                }
                break;
            case 'M':
#ifndef _UNIX
                ok_modem_stuff = false;
#endif
                break;
            case 'R':
                num_min = atoi(&(s[2]));
                break;
            case 'U':
                this_usernum = wwiv::stringUtils::StringToUnsignedShort(&(s[2]));
                if ( !m_bUserAlreadyOn )
                {
                    sess->SetCurrentSpeed( "KB" );
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
                if ( wwiv::UpperCase<char>( s[2] ) == 'T' || wwiv::UpperCase<char>( s[2] ) == 'C' )
                {
                    // This more of a hack to make sure the Telnet
                    // Server's -Bxxx parameter doesn't hose us.
                    sess->SetCurrentSpeed( "115200" );
                    sess->SetUserOnline( false );
                    us                  = 115200U;
                    ui                  = us;
                    m_bUserAlreadyOn    = true;
                    ooneuser            = true;
                    sess->using_modem   = 0;
                    hangup              = false;
                    incom               = true;
                    outcom              = false;
                    global_xx           = false;
                }
                if ( wwiv::UpperCase<char>( s[2] ) == 'C' )
                {
                    bTelnetInstance = false;
                }
                else if ( wwiv::UpperCase<char>(s[2] ) == 'T' )
                {
                    bTelnetInstance = true;
                }
                else
                {
                    strcpy( s, argv[i] );
                    std::cout << "Invalid Command line argument given '" << s << "'\r\n\n";
                    exit( m_nErrorLevel );
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
					GetLocalIO()->LocalCls();
					if ( ( i + 1 ) < argc )
					{
						i++;
						sess->bout << "\r\n|#7\xFE |10Packing specified subs: \r\n";
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
						sess->bout << "\r\n|#7\xFE |10Packing all subs: \r\n";
						sysoplogf( "* Packing All Message Areas" );
						pack_all_subs( true );
					}
					this->ShutdownBBS();
				}
				break;
            case '?':
                ShowUsage();
                exit( 0 );
                break;
			case '-':
				{
					if ( s[0] == '-' )
					{
                        if ( wwiv::stringUtils::IsEqualsIgnoreCase( &s[2], "help" ) )
						{
							ShowUsage();
							exit( 0 );
							break;
						}
					}
				}
            default:
                strcpy( s, argv[i] );
                std::cout << "Invalid Command line argument given '" << s << "'\r\n\n";
                exit( m_nErrorLevel );
                break;
            }
        }
        else
        {
            // Command line argument did not start with a '-' or a '/'
            std::cout << "Invalid Command line argument given '" << argv[ i ] << "'\r\n\n";
            exit( m_nErrorLevel );
        }
    }

    // Add the environment variable or overwrite the existing one
    char szInstanceEnvVar[81];
#ifndef _UNIX
    snprintf( szInstanceEnvVar, sizeof( szInstanceEnvVar ), "WWIV_INSTANCE=%ld", GetInstanceNumber() );
    _putenv( szInstanceEnvVar );
#else
    // For some reason putenv() doesn't work sometimes when setenv() does...
    snprintf( szInstanceEnvVar, sizeof( szInstanceEnvVar ), "%u", GetInstanceNumber() );
    setenv( "WWIV_INSTANCE", szInstanceEnvVar, 1 );
    m_bUserAlreadyOn = true;
#endif

#if defined ( _WIN32 )

    if ( bTelnetInstance )
    {
        // If this is a telnet Node...
		sess->hCommHandle   = static_cast<HANDLE>( NULL );
        sess->hSocket		= static_cast<SOCKET>( hSockOrComm );
		if ( !DuplicateHandle(  GetCurrentProcess(), reinterpret_cast<HANDLE>( sess->hSocket ),
			                    GetCurrentProcess(), reinterpret_cast<HANDLE*>( &sess->hDuplicateSocket ),
			                    0, TRUE, DUPLICATE_SAME_ACCESS ) )
        {
			std::cout << "Error creating duplicate socket: " << GetLastError() << "\r\n\n";
		}
    }
    else
    {
		sess->hSocket		= (SOCKET)( NULL );
        sess->hCommHandle   = (HANDLE)( hSockOrComm );
    }

#endif
    StartupComm( bTelnetInstance );
    this->InitializeBBS();

    if ( szSystemPassword[0] )
    {
        strcpy( syscfg.systempw, szSystemPassword );
    }

    if ( syscfg.sysconfig & sysconfig_no_local )
    {
        this_usernum = 0;
        m_bUserAlreadyOn = false;
    }
    if ( szFullResultCode[0] )
    {
        process_full_result( szFullResultCode );
    }

#if defined (_WIN32)
    // Set console title
    std::stringstream consoleTitleStream;
    consoleTitleStream << "WWIV Node " << GetInstanceNumber() << " (" << syscfg.systemname << ")";
    SetConsoleTitle( consoleTitleStream.str().c_str() );

    // If we are telnet...
    if ( sess->hSocket )
    {
	    // If the com port is set to 0 here, then ok_modem_stuff is cleared
	    // in the call to init.  Well.. If we are running native sockets, we
	    // could care less about the com port... So set it back to true here
	    // ... the better solution would just be to tell init to "get bent"
	    ok_modem_stuff = true;
        comm->open();

        SOCKADDR_IN addr;
        int nAddrSize = sizeof( SOCKADDR);

        getpeername( sess->hSocket, reinterpret_cast<SOCKADDR *>( &addr ), &nAddrSize );

        char * pszIPAddress = inet_ntoa( addr.sin_addr );
        strcpy( cid_num, pszIPAddress );
        strcpy( cid_name, "Internet TELNET Session" );

        char szTempTelnet[ 21 ];
        snprintf( szTempTelnet, sizeof( szTempTelnet ), "%c%c%c", WIOTelnet::TELNET_OPTION_IAC, WIOTelnet::TELNET_OPTION_DONT, WIOTelnet::TELNET_OPTION_ECHO );
        comm->write( szTempTelnet, 3, true );
        snprintf( szTempTelnet, sizeof( szTempTelnet ), "%c%c%c", WIOTelnet::TELNET_OPTION_IAC, WIOTelnet::TELNET_OPTION_WILL, WIOTelnet::TELNET_OPTION_ECHO );
        comm->write( szTempTelnet, 3, true );
        snprintf( szTempTelnet, sizeof( szTempTelnet ), "%c%c%c", WIOTelnet::TELNET_OPTION_IAC, WIOTelnet::TELNET_OPTION_DONT, WIOTelnet::TELNET_OPTION_LINEMODE );
        comm->write( szTempTelnet, 3, true );
    }
#endif

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
        cleanup_events();
        if ( !wwiv::stringUtils::IsEquals( date(), status.date1 ) )
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
            sess->usernum = this_usernum;
            sess->ReadCurrentUser( sess->usernum );
            if ( !sess->thisuser.isUserDeleted() )
            {
                GotCaller( ui, us );
                sess->usernum = this_usernum;
                sess->ReadCurrentUser( sess->usernum );
                read_qscn( sess->usernum, qsc, false );
                sess->ResetEffectiveSl();
                changedsl();
                okmacro = true;
                if ( !hangup && sess->usernum > 0 &&
                     sess->thisuser.isRestrictionLogon() &&
                     wwiv::stringUtils::IsEquals( date(), sess->thisuser.GetLastOn() ) &&
                     sess->thisuser.GetTimesOnToday() > 0 )
                {
                    sess->bout << "\r\n|12Sorry, you can only logon once per day.\r\n\n";
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
#ifndef _UNIX
            else
            {
                this->GetCaller();
            }
#endif
        }

        if ( modem_mode == mode_fax )
        {
            if ( WFile::ExistsWildcard( "WWIVFAX.*" ) )
            {
                char szCommand[ MAX_PATH ];
                stuff_in( szCommand, "WWIVFAX %S %P", "", "", "", "", "" );
                ExecuteExternalProgram( szCommand, EFLAG_NONE );
            }
            goto hanging_up;
        }
        if ( sess->using_modem > -1 )
        {
            if ( !sess->using_modem )
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
            sess->using_modem = 0;
            okmacro = true;
            sess->usernum = m_unx;
            sess->ResetEffectiveSl();
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
                write_inst( INST_LOC_MAIN, usub[sess->GetCurrentMessageArea()].subnum, INST_FLAGS_NONE );
                mainmenu();
            }
            logoff();
        }
    hanging_up:

        if ( !no_hangup && sess->using_modem && ok_modem_stuff )
        {
            hang_it_up();
        }
        catsl();
        frequent_init();
        if ( sess->wfc_status == 0 )
        {
            GetLocalIO()->LocalCls();
        }
        cleanup_net();

        if ( !sess->using_modem )
        {
            holdphone( false );
        }
        if ( !no_hangup && ok_modem_stuff )
        {
            comm->dtr( false );
        }
        m_bUserAlreadyOn = false;
        if ( GetLocalIO()->GetSysopAlert() && (!GetLocalIO()->LocalKeyPressed() ) )
        {
            comm->dtr( true );
            wait1( 2 );
            holdphone( true );
            double dt = timer();
            GetLocalIO()->LocalCls();
            sess->bout << "\r\n>> SYSOP ALERT ACTIVATED <<\r\n\n";
            while ( !GetLocalIO()->LocalKeyPressed() && ( fabs( timer() - dt ) < SECONDS_PER_MINUTE_FLOAT ) )
            {
				WWIV_Sound( 500, 250 );
				WWIV_Delay( 1 );
            }
            GetLocalIO()->LocalCls();
            holdphone( false );
        }
        GetLocalIO()->SetSysopAlert( false );
    } while ( !ooneuser );

    return m_nOkLevel;
}



void WBbsApp::ShowUsage()
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



WBbsApp::WBbsApp()
{
    comm			    = NULL;
    sess			    = new WSession( this );
    localIO			    = new WLocalIO();
    statusMgr			    = new StatusMgr();
    userManager			    = new WUserManager();
    m_nOkLevel			    = WBbsApp::exitLevelOK;
    m_nErrorLevel		    = WBbsApp::exitLevelNotOK;
    m_nInstance			    = 1;
	m_bUserAlreadyOn	    = false;
    m_nBbsShutdownStatus    = WBbsApp::shutdownNone;
    m_fShutDownTime         = 0.0;

    WFile::SetLogger( this );
    WFile::SetDebugLevel( sess->GetGlobalDebugLevel() );

    tzset();

	// Set the home directory
	getcwd( m_szCurrentDirectory, MAX_PATH );

}


WBbsApp::WBbsApp( const WBbsApp& copy )
{
    comm = copy.comm;
    localIO = copy.localIO;
    statusMgr = copy.statusMgr;
    userManager = copy.userManager;
    m_nOkLevel = copy.m_nOkLevel;
    m_nErrorLevel = copy.m_nErrorLevel;
    m_nInstance = copy.m_nInstance;
    m_bUserAlreadyOn = copy.m_bUserAlreadyOn;
    m_fShutDownTime = copy.m_fShutDownTime;
    
    strcpy( m_szCurrentDirectory, copy.m_szCurrentDirectory );
    
}


void WBbsApp::CdHome()
{
	WWIV_ChangeDirTo( m_szCurrentDirectory );
}


const char* WBbsApp::GetHomeDir()
{
	static char szDir[ MAX_PATH ];
	snprintf( szDir, sizeof( szDir ), "%s%c", m_szCurrentDirectory, WWIV_FILE_SEPERATOR_CHAR );
	return szDir;
}


bool WBbsApp::StartupComm(bool bUseSockets)
{
    if ( NULL != comm )
    {
        std::cout << "Cannot startup comm support, it's already started!!\r\n";
        return false;
    }

#if defined ( _WIN32 )

    if ( bUseSockets )
    {
        comm = new WIOTelnet();
    }
    else
    {
        comm = new WIOSerial();
    }

#elif defined ( _UNIX )

    comm = new WIOUnix();

#elif defined ( __OS2 )

#error "You must implement the stuff to write with!!!"

#endif // defined ($PLATFORM)

    return comm->startup();
}


bool WBbsApp::ShutdownComm()
{
	if ( NULL == comm )
	{
        std::cout << "Cannot shutdown comm support, it's not started!!\r\n";
		return false;
	}

	bool ret = comm->shutdown();
    delete comm;
    return ret;
}


void WBbsApp::AbortBBS( bool bSkipShutdown )
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


void WBbsApp::ShutdownBBS()
{
    ExitBBSImpl( m_nOkLevel );
}


void WBbsApp::QuitBBS()
{
    ExitBBSImpl( WBbsApp::exitLevelQuit );
}


void WBbsApp::ExitBBSImpl( int nExitLevel )
{
    localIO->LocalCls();

    char szBuffer[81];
    snprintf( szBuffer, sizeof( szBuffer ), "WWIV %s, inst %u, taken down at %s on %s with exit code %d.",
			  wwiv_version, GetInstanceNumber(), times(), fulldate(), nExitLevel );
    sysoplog( "", false );
    sysoplog( szBuffer, false );
    sysoplog( "", false );
    catsl();
    close_strfiles();
    write_inst( INST_LOC_DOWN, 0, INST_FLAGS_NONE );
    if ( ok_modem_stuff && comm != NULL )
    {
        comm->close();
	if ( comm != NULL )
	{
	    delete comm;
	    comm = NULL;
	}
    }
    char szMessage[ 255 ];
    snprintf( szMessage, sizeof( szMessage ), "WWIV Bulletin Board System %s%s exiting at error level %d\r\n\n", wwiv_version, beta_version, nExitLevel );
    localIO->LocalPuts( szMessage );
    localIO->SetCursor( WLocalIO::cursorNormal );
    delete this;
    exit( nExitLevel );

}


bool WBbsApp::LogMessage( const char* pszFormat, ... )
{
    va_list ap;
    char szBuffer[2048];

    va_start( ap, pszFormat );
    vsnprintf( szBuffer, sizeof( szBuffer ), pszFormat, ap );
    va_end( ap );
    sysoplog( szBuffer );
    return true;
}


WBbsApp::~WBbsApp()
{
    if ( comm != NULL )
    {
		comm->shutdown();
        delete comm;
        comm = NULL;
    }

    if ( sess != NULL )
    {
        delete sess;
        sess = NULL;
    }

    if ( localIO != NULL )
    {
        delete localIO;
        localIO = NULL;
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
    app = new WBbsApp();
    int nRetCode = GetApplication()->Run( argc, argv );
    return nRetCode;
}






