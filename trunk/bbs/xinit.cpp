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

// Additional INI file function and structure
#include "xinitini.h"


// %%TODO: Turn this warning back on once the rest of the code is cleaned up
#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4244 )
#endif

#ifdef _UNIX
#define XINIT_PRINTF( x )
#else
#define XINIT_PRINTF( x ) std::cout << ( x )
#endif // _UNIX

#define INI_STRFILE 7

struct ini_flags_type
{
    int			strnum;
    bool		sense;
    unsigned long value;
};

unsigned long GetFlagsFromIniFile(WIniFile *pIniFile, ini_flags_type * fs, int num, unsigned long flags);


// Turns a string into a bitmapped unsigned short flag for use with
// ExecuteExternalProgram calls.
unsigned short WApplication::str2spawnopt( const char *s )
{
    char ts[ 255 ];

    unsigned short return_val = 0;
    strcpy( ts, s );
    WWIV_STRUPR( ts );

    if (strstr(ts, "ABORT") != NULL)
    {
        return_val |= EFLAG_ABORT;
    }
    if (strstr(ts, "INTERNAL") != NULL)
    {
        return_val |= EFLAG_INTERNAL;
    }
    if (strstr(ts, "NOHUP") != NULL)
    {
        return_val |= EFLAG_NOHUP;
    }
    if (strstr(ts, "COMIO") != NULL)
    {
        return_val |= EFLAG_COMIO;
    }
    if (strstr(ts, "FOSSIL") != NULL)
    {
        return_val |= EFLAG_FOSSIL; // RF20020929
    }
    if (strstr(ts, "NOPAUSE") != NULL)
    {
        return_val |= EFLAG_NOPAUSE;
    }
    if (strstr(ts, "NETPROG") != NULL)
    {
        return_val |= EFLAG_NETPROG;
    }
    if (strstr(ts, "TOPSCREEN") != NULL)
    {
        return_val |= EFLAG_TOPSCREEN;
    }

    return return_val;
}


// Takes string s and creates restrict val
unsigned short WApplication::str2restrict( const char *s )
{
    char *rs = restrict_string;
    char s1[81];

    strcpy(s1, s);
    WWIV_STRUPR(s1);
    int r = 0;
    for ( int i = strlen(rs) - 1; i >= 0; i-- )
    {
        if ( strchr( s1, rs[i] ) )
        {
            r |= ( 1 << i );
        }
    }

    return static_cast< short >( r );
}


// begin callback addition

unsigned char WApplication::stryn2tf(const char *s)
{
	char ch = wwiv::UpperCase<char>( *s );

	char yesChar = *( YesNoString( true ) );
	if ( ch == yesChar || ch == 1 || ch == '1' || ch == 'T' )
	{
		return 1;
	}
	return 0;
}


// end callback addition


#define OFFOF(x) ( reinterpret_cast<long>( &GetSession()->GetCurrentUser()->data.x ) - reinterpret_cast<long>( &GetSession()->GetCurrentUser()->data ) )


// Reads WWIV.INI info from [WWIV] subsection, overrides some config.dat
// settings (as appropriate), updates config.dat with those values. Also
// tries to read settings from [WWIV-<instnum>] subsection - this overrides
// those in [WWIV] subsection.


static unsigned char nucol[] = {7, 11, 14, 5, 31, 2, 12, 9, 6, 3};
static unsigned char nucolbw[] = {7, 15, 15, 15, 112, 15, 15, 7, 7, 7};


struct eventinfo_t
{
    char *name;
    unsigned short eflags;
};


static eventinfo_t eventinfo[] =
{
    {"TIMED",         0},
    {"NEWUSER",       0},
    {"BEGINDAY",      0},
    {"LOGON",         EFLAG_COMIO | EFLAG_INTERNAL},
    {"ULCHK",         EFLAG_NOHUP},
    {"FSED",          EFLAG_COMIO},
    {"PROT_SINGLE",   0},
    {"PROT_BATCH",    EFLAG_TOPSCREEN},
    {"CHAT",          0},
    {"ARCH_E",        EFLAG_COMIO | EFLAG_INTERNAL},
    {"ARCH_L",        EFLAG_COMIO | EFLAG_INTERNAL | EFLAG_ABORT},
    {"ARCH_A",        EFLAG_COMIO | EFLAG_INTERNAL},
    {"ARCH_D",        EFLAG_COMIO | EFLAG_INTERNAL},
    {"ARCH_K",        EFLAG_COMIO | EFLAG_INTERNAL},
    {"ARCH_T",        EFLAG_COMIO | EFLAG_INTERNAL},
    {"NET_CMD1",      EFLAG_INTERNAL},
    {"NET_CMD2",      EFLAG_NETPROG | EFLAG_INTERNAL},
    {"LOGOFF",        EFLAG_COMIO | EFLAG_INTERNAL},
    {"V_SCAN",        EFLAG_NOPAUSE},
    {"NETWORK",       0},
};


static const char *get_key_str( int n, const char *index = NULL )
{
    static char str[255];
    if ( !index )
    {
	    return INI_OPTIONS_ARRAY[ n ];
    }
    sprintf( str, "%s[%s]", INI_OPTIONS_ARRAY[ n ], index );
    return str;
}


#define INI_GET_ASV(s, f, func, d) \
{if ((ss=iniFile.GetValue(get_key_str(INI_STR_SIMPLE_ASV,s)))!=NULL) GetSession()->asv.f = func (ss); else GetSession()->asv.f = d;}
#define INI_GET_CALLBACK(s, f, func, d) \
{if ((ss=iniFile.GetValue(get_key_str(INI_STR_CALLBACK,s)))!=NULL) \
    GetSession()->cbv.f = func (ss); \
    else GetSession()->cbv.f = d;}

#define NEL(s) (sizeof(s) / sizeof((s)[0]))


static ini_flags_type sysinfo_flags[] =
{
    {INI_STR_FORCE_FBACK, false, OP_FLAGS_FORCE_NEWUSER_FEEDBACK},
    {INI_STR_CHECK_DUP_PHONES, false, OP_FLAGS_CHECK_DUPE_PHONENUM},
    {INI_STR_HANGUP_DUP_PHONES, false, OP_FLAGS_HANGUP_DUPE_PHONENUM},
    {INI_STR_USE_SIMPLE_ASV, false, OP_FLAGS_SIMPLE_ASV},
    {INI_STR_POSTTIME_COMPENS, false, OP_FLAGS_POSTTIME_COMPENSATE},
    {INI_STR_SHOW_HIER, false, OP_FLAGS_SHOW_HIER},
    {INI_STR_IDZ_DESC, false, OP_FLAGS_IDZ_DESC},
    {INI_STR_SETLDATE, false, OP_FLAGS_SETLDATE},
    {INI_STR_SLASH_SZ, false, OP_FLAGS_SLASH_SZ},
    {INI_STR_READ_CD_IDZ, false, OP_FLAGS_READ_CD_IDZ},
    {INI_STR_FSED_EXT_DESC, false, OP_FLAGS_FSED_EXT_DESC},
    {INI_STR_FAST_TAG_RELIST, false, OP_FLAGS_FAST_TAG_RELIST},
    {INI_STR_MAIL_PROMPT, false, OP_FLAGS_MAIL_PROMPT},
    {INI_STR_SHOW_CITY_ST, false, OP_FLAGS_SHOW_CITY_ST},
    {INI_STR_NO_EASY_DL, false, OP_FLAGS_NO_EASY_DL},
    {INI_STR_NEW_EXTRACT, false, OP_FLAGS_NEW_EXTRACT},
    {INI_STR_FAST_SEARCH, false, OP_FLAGS_FAST_SEARCH},
    {INI_STR_NET_CALLOUT, false, OP_FLAGS_NET_CALLOUT},
    {INI_STR_WFC_SCREEN, false, OP_FLAGS_WFC_SCREEN},
    {INI_STR_FIDO_PROCESS, false, OP_FLAGS_FIDO_PROCESS},
    {INI_STR_USER_REGISTRATION, false, OP_FLAGS_USER_REGISTRATION},
    {INI_STR_MSG_TAG, false, OP_FLAGS_MSG_TAG},
    {INI_STR_CHAIN_REG, false, OP_FLAGS_CHAIN_REG},
    {INI_STR_CAN_SAVE_SSM, false, OP_FLAGS_CAN_SAVE_SSM},
    {INI_STR_EXTRA_COLOR, false, OP_FLAGS_EXTRA_COLOR},
    {INI_STR_THREAD_SUBS, false, OP_FLAGS_THREAD_SUBS},
    {INI_STR_USE_CALLBACK, false, OP_FLAGS_CALLBACK},
    {INI_STR_USE_VOICE_VAL, false, OP_FLAGS_VOICE_VAL},
    {INI_STR_USE_ADVANCED_ASV, false, OP_FLAGS_ADV_ASV},
    {INI_STR_USE_FORCE_SCAN, false, OP_FLAGS_USE_FORCESCAN},
    {INI_STR_NEWUSER_MIN, false, OP_FLAGS_NEWUSER_MIN},

};

static ini_flags_type sysconfig_flags[] =
{
    {INI_STR_LOCAL_SYSOP, true, sysconfig_no_local},
    {INI_STR_2WAY_CHAT, false, sysconfig_2_way},
    {INI_STR_OFF_HOOK, false, sysconfig_off_hook},
    {INI_STR_TITLEBAR, false, sysconfig_titlebar},
    {INI_STR_LOG_DOWNLOADS, false, sysconfig_log_dl},
    {INI_STR_CLOSE_XFER, false, sysconfig_no_xfer},
    {INI_STR_ALL_UL_TO_SYSOP, false, sysconfig_all_sysop},
    {INI_STR_BEEP_CHAT, true, sysconfig_no_beep},
    {INI_STR_TWO_COLOR_CHAT, false, sysconfig_two_color},
    {INI_STR_ALLOW_ALIASES, true, sysconfig_no_alias},
    {INI_STR_USE_LIST, false, sysconfig_list},
    {INI_STR_EXTENDED_USERINFO, false, sysconfig_extended_info},
    {INI_STR_FREE_PHONE, false, sysconfig_free_phone},
    {INI_STR_ENABLE_PIPES, false, sysconfig_enable_pipes},
    {INI_STR_ENABLE_MCI, false, sysconfig_enable_mci},
};


bool WApplication::ReadINIFile()
{
    // can't allow user to change these on-the-fly
    unsigned short omb = GetSession()->max_batch;
    unsigned short omc = GetSession()->max_chains;
    unsigned short omg = GetSession()->max_gfilesec;

    // Setup default GetSession()-> data

    for ( int nTempColorNum = 0; nTempColorNum < 10; nTempColorNum++ )
    {
        GetSession()->newuser_colors[ nTempColorNum ] = nucol[ nTempColorNum ];
        GetSession()->newuser_bwcolors[ nTempColorNum ] = nucolbw[ nTempColorNum ];
    }

    GetSession()->SetTopScreenColor( 31 );
    GetSession()->SetUserEditorColor( 31 );
    GetSession()->SetEditLineColor( 112 );
    GetSession()->SetChatNameSelectionColor( 95 );
    GetSession()->SetMessageColor( 2 );
    GetSession()->max_batch = 50;
    GetSession()->max_extend_lines = 10;
    GetSession()->max_chains = 50;
    GetSession()->max_gfilesec = 32;
    GetSession()->mail_who_field_len = 35;
    GetSession()->SetBeginDayNodeNumber( 1 );
    GetSession()->SetUseInternalZmodem( true );
	GetSession()->SetExecUseWaitForInputIdle( true );
    GetSession()->SetExecWaitForInputTimeout( 2000 );
	GetSession()->SetExecChildProcessWaitTime( 500 );
	GetSession()->SetExecLogSyncFoss( true );
	GetSession()->SetNewScanAtLogin( false );

    for ( size_t nTempEventNum = 0; nTempEventNum < NEL( eventinfo ); nTempEventNum++ )
    {
        GetApplication()->spawn_opts[ nTempEventNum ] = eventinfo[ nTempEventNum ].eflags;
    }

    // put in default WApplication::flags
    GetApplication()->SetConfigFlags( OP_FLAGS_FIDO_PROCESS );

    if ( ok_modem_stuff )
    {
        GetApplication()->SetConfigFlag( OP_FLAGS_NET_CALLOUT );
    }
    else
    {
        GetApplication()->ClearConfigFlag( OP_FLAGS_NET_CALLOUT );
    }

    // initialize ini communication
    char szInstanceName[255];
    snprintf( szInstanceName, sizeof( szInstanceName ), "WWIV-%u", GetInstanceNumber() );
    WIniFile iniFile( WWIV_INI );
    if ( iniFile.Open( szInstanceName, INI_TAG ) )
    {
        ///////////////////////////////////////////////////////////////////////////////
        // DO NOT DO ANYTHING HERE WHICH ALLOCATES MEMORY
        // the ini_init has allocated a lot of memory, and it will be freed
        // by ini_done.  If you need to read anything here which will cause memory
        // to be allocated, only store the size of the array here, and allocate it
        // after ini_done.  Or, if necessary, call ini_done(), allocate the memory,
        // then call ini_init again, and continue processing.
        ///////////////////////////////////////////////////////////////////////////////

        //
        // found something
        //

        // pull out event flags
        const char *ss;
        for ( size_t nTempSpawnOptNum = 0; nTempSpawnOptNum < NEL( GetApplication()->spawn_opts ); nTempSpawnOptNum++ )
        {
            char szKeyName[255];
            sprintf( szKeyName, "%s[%s]", get_key_str( INI_STR_SPAWNOPT ), eventinfo[nTempSpawnOptNum].name );
            if ( ( ss = iniFile.GetValue( szKeyName ) ) != NULL )
            {
                GetApplication()->spawn_opts[nTempSpawnOptNum] = str2spawnopt( ss );
            }
        }

        // pull out newuser colors
        for ( int nTempColorNum = 0; nTempColorNum < 10; nTempColorNum++ )
        {
            char szKeyName[255];
            sprintf( szKeyName, "%s[%d]", get_key_str( INI_STR_NUCOLOR ), nTempColorNum );
            if ( ( ss = iniFile.GetValue( szKeyName ) ) != NULL && atoi( ss ) > 0 )
            {
                GetSession()->newuser_colors[nTempColorNum] = atoi( ss );
            }
            sprintf( szKeyName, "%s[%d]", get_key_str( INI_STR_NUCOLOR ), nTempColorNum );
            if ( ( ss = iniFile.GetValue( szKeyName ) ) != NULL && atoi( ss ) > 0 )
            {
                GetSession()->newuser_bwcolors[nTempColorNum] = atoi( ss );
            }
        }

        // pull out sysop-side colors
        GetSession()->SetTopScreenColor( iniFile.GetNumericValue( get_key_str ( INI_STR_TOPCOLOR ), GetSession()->GetTopScreenColor() ) );
        GetSession()->SetUserEditorColor( iniFile.GetNumericValue( get_key_str ( INI_STR_F1COLOR ), GetSession()->GetUserEditorColor() ) );
        GetSession()->SetEditLineColor( iniFile.GetNumericValue( get_key_str ( INI_STR_EDITLINECOLOR ), GetSession()->GetEditLineColor() ) );
        GetSession()->SetChatNameSelectionColor( iniFile.GetNumericValue( get_key_str ( INI_STR_CHATSELCOLOR ), GetSession()->GetChatNameSelectionColor() ) );

        // pull out sizing options
        GetSession()->max_batch = iniFile.GetNumericValue( get_key_str ( INI_STR_MAX_BATCH ), GetSession()->max_batch );
        GetSession()->max_extend_lines = iniFile.GetNumericValue( get_key_str ( INI_STR_MAX_EXTEND_LINES ), GetSession()->max_extend_lines );
        GetSession()->max_gfilesec = iniFile.GetNumericValue( get_key_str ( INI_STR_MAX_CHAINS ), GetSession()->max_gfilesec );
        GetSession()->max_gfilesec = iniFile.GetNumericValue( get_key_str ( INI_STR_MAX_GFILESEC ), GetSession()->max_gfilesec );

        // pull out strings
        //    INI_INIT_STR(INI_STR_TERMINAL_CMD, terminal);
        //    INI_INIT_STR(INI_STR_EXECUTE_CMD, executestr);
        //    INI_INIT_STR(INI_STR_UPLOAD_CMD, upload_c);
        //    INI_INIT_STR(INI_STR_BEGINDAY_CMD, beginday_c);
        //    INI_INIT_STR(INI_STR_NEWUSER_CMD, newuser_c);
        //    INI_INIT_STR(INI_STR_LOGON_CMD, logon_c);
        //    INI_INIT_STR(INI_STR_LOGOFF_CMD, logoff_c);

        GetSession()->m_nForcedReadSubNumber = iniFile.GetNumericValue(get_key_str(INI_STR_FORCE_SCAN_SUBNUM),GetSession()->m_nForcedReadSubNumber);
        GetSession()->m_bInternalZmodem = iniFile.GetBooleanValue( get_key_str(INI_STR_INTERNALZMODEM), GetSession()->m_bInternalZmodem );
        GetSession()->m_bNewScanAtLogin = iniFile.GetBooleanValue( get_key_str(INI_STR_NEW_SCAN_AT_LOGIN), GetSession()->m_bNewScanAtLogin );

        GetSession()->m_bExecLogSyncFoss = iniFile.GetBooleanValue( get_key_str(INI_STR_EXEC_LOG_SYNCFOSS), GetSession()->m_bExecLogSyncFoss );
        GetSession()->m_bExecUseWaitForInputIdle = iniFile.GetBooleanValue( get_key_str(INI_STR_EXEC_USE_WAIT_FOR_IDLE), GetSession()->m_bExecUseWaitForInputIdle );
        GetSession()->m_nExecChildProcessWaitTime = iniFile.GetNumericValue(get_key_str(INI_STR_EXEC_CHILD_WAIT_TIME),GetSession()->m_nExecChildProcessWaitTime);
        GetSession()->m_nExecUseWaitForInputTimeout = iniFile.GetNumericValue(get_key_str(INI_STR_EXEC_WAIT_FOR_IDLE_TIME),GetSession()->m_nExecUseWaitForInputTimeout);

        GetSession()->SetBeginDayNodeNumber( iniFile.GetNumericValue( get_key_str ( INI_STR_BEGINDAYNODENUMBER ), GetSession()->GetBeginDayNodeNumber() ) );

        // pull out sysinfo_flags
        GetApplication()->SetConfigFlags( GetFlagsFromIniFile( &iniFile, sysinfo_flags, NEL( sysinfo_flags ), GetApplication()->GetConfigFlags() ) );

        // allow override of WSession::m_nMessageColor
        GetSession()->SetMessageColor( iniFile.GetNumericValue( get_key_str ( INI_STR_MSG_COLOR ), GetSession()->GetMessageColor() ) );

        // get asv values
        if ( GetApplication()->HasConfigFlag( OP_FLAGS_SIMPLE_ASV ) )
        {
            INI_GET_ASV("SL", sl, atoi, syscfg.autoval[9].sl);
            INI_GET_ASV("DSL", dsl, atoi, syscfg.autoval[9].dsl);
            INI_GET_ASV("EXEMPT", exempt, atoi, 0);
            INI_GET_ASV("AR", ar, str_to_arword, syscfg.autoval[9].ar);
            INI_GET_ASV("DAR", dar, str_to_arword, syscfg.autoval[9].dar);
            INI_GET_ASV("RESTRICT", restrict, str2restrict, 0);
        }
        if ( GetApplication()->HasConfigFlag( OP_FLAGS_ADV_ASV ) )
        {
            GetSession()->advasv.reg_wwiv = iniFile.GetNumericValue(get_key_str(INI_STR_ADVANCED_ASV,"REG_WWIV"), 1);
            GetSession()->advasv.nonreg_wwiv = iniFile.GetNumericValue(get_key_str(INI_STR_ADVANCED_ASV,"NONREG_WWIV"), 1);
            GetSession()->advasv.non_wwiv = iniFile.GetNumericValue(get_key_str(INI_STR_ADVANCED_ASV,"NON_WWIV"), 1);
            GetSession()->advasv.cosysop = iniFile.GetNumericValue(get_key_str(INI_STR_ADVANCED_ASV,"COSYSOP"), 1);
        }


        // get callback values
        if (GetApplication()->HasConfigFlag( OP_FLAGS_CALLBACK ) )
        {
            INI_GET_CALLBACK("SL", sl, atoi, syscfg.autoval[2].sl);
            INI_GET_CALLBACK("DSL", dsl, atoi, syscfg.autoval[2].dsl);
            INI_GET_CALLBACK("EXEMPT", exempt, atoi, 0);
            INI_GET_CALLBACK("AR", ar, str_to_arword, syscfg.autoval[2].ar);
            INI_GET_CALLBACK("DAR", dar, str_to_arword, syscfg.autoval[2].dar);
            INI_GET_CALLBACK("RESTRICT", restrict, str2restrict, 0);
            INI_GET_CALLBACK("FORCED", forced, stryn2tf, 0);
            INI_GET_CALLBACK("LONG_DISTANCE", longdistance, stryn2tf, 0);
            INI_GET_CALLBACK("REPEAT", repeat, atoi, 0);
        }


        // sysconfig flags
        syscfg.sysconfig = static_cast<unsigned short>( GetFlagsFromIniFile( &iniFile, sysconfig_flags,
                            NEL( sysconfig_flags ), syscfg.sysconfig ) );

        // misc stuff
        if ( ( ( ss = iniFile.GetValue( get_key_str( INI_STR_MAIL_WHO_LEN ) ) ) != NULL ) &&
            ( atoi(ss) > 0 || ss[0] == '0' ) )
        {
            GetSession()->mail_who_field_len = atoi( ss );
        }
        if ( ( ss = iniFile.GetValue( get_key_str( INI_STR_RATIO ) ) ) != NULL )
        {
            syscfg.req_ratio = static_cast<float>( atof( ss ) );
        }

        if ( ( ss = iniFile.GetValue( get_key_str( INI_STR_ATTACH_DIR ) ) ) != NULL )
        {
            m_attachmentDirectory = ss;
            if ( m_attachmentDirectory.at( m_attachmentDirectory.length() - 1 ) != WWIV_FILE_SEPERATOR_CHAR )
            {
                m_attachmentDirectory += WWIV_FILE_SEPERATOR_STRING;
            }
        }
        else
        {
            m_attachmentDirectory = GetHomeDir();
            m_attachmentDirectory += ATTACH_DIR;
            m_attachmentDirectory += WWIV_FILE_SEPERATOR_STRING;
        }

        GetSession()->screen_saver_time = iniFile.GetNumericValue(get_key_str(INI_STR_SCREEN_SAVER_TIME),GetSession()->screen_saver_time);

        // now done
        iniFile.Close();
    }

	GetSession()->max_extend_lines    = std::min<unsigned short>( GetSession()->max_extend_lines, 99 );
    GetSession()->max_batch           = std::min<unsigned short>( GetSession()->max_batch , 999 );
    GetSession()->max_chains          = std::min<unsigned short>( GetSession()->max_chains, 999 );
    GetSession()->max_gfilesec        = std::min<unsigned short>( GetSession()->max_gfilesec, 999 );

    if ( omb )
	{
        GetSession()->max_batch = omb;
	}
    if ( omc )
	{
        GetSession()->max_chains = omc;
	}
    if ( omg )
	{
       GetSession()->max_gfilesec = omg;
	}

    set_strings_fn( INI_STRFILE, NULL, NULL, 0 );

    // Success if here
    return true;
}


bool WApplication::ReadConfig()
{
    configrec full_syscfg;

    WFile configFile( CONFIG_DAT );
	int nFileMode = WFile::modeReadOnly | WFile::modeBinary;
    if ( !configFile.Open( nFileMode ) )
    {
        std::cout << CONFIG_DAT << " NOT FOUND.\r\n";
        return false;
    }
    configFile.Read( &full_syscfg, sizeof( configrec ) );
    configFile.Close();

    WFile configOvrFile( CONFIG_OVR );
    bool bIsConfigObvOpen = configOvrFile.Open( WFile::modeBinary | WFile::modeReadOnly );
    if (  bIsConfigObvOpen &&
         configOvrFile.GetLength() < static_cast<long>( GetInstanceNumber() * sizeof( configoverrec ) ) )
    {
        configOvrFile.Close();
        std::cout << "Not enough instances configured.\r\n";
        AbortBBS();
    }
    if ( !bIsConfigObvOpen )
    {
        // slap in the defaults, although this is not used anymore
        for ( int nTempPortNum = 0; nTempPortNum < 4; nTempPortNum++ )
        {
            syscfgovr.com_ISR[ nTempPortNum + 1 ]   = full_syscfg.com_ISR[ nTempPortNum + 1 ];
            syscfgovr.com_base[ nTempPortNum + 1 ]  = full_syscfg.com_base[ nTempPortNum + 1 ];
            syscfgovr.com_ISR[ nTempPortNum + 5 ]   = full_syscfg.com_ISR[ nTempPortNum + 1 ];
            syscfgovr.com_base[ nTempPortNum + 5 ]  = full_syscfg.com_base[ nTempPortNum + 1 ];
        }

        syscfgovr.primaryport = full_syscfg.primaryport;
        strcpy( syscfgovr.modem_type, full_syscfg.modem_type );
        strcpy( syscfgovr.tempdir, full_syscfg.tempdir );
        strcpy( syscfgovr.batchdir, full_syscfg.batchdir );
    }
    else
    {
        long lCurNodeOffset = ( GetInstanceNumber() - 1) * sizeof(configoverrec);
        configOvrFile.Seek( lCurNodeOffset, WFile::seekBegin );
        configOvrFile.Read( &syscfgovr, sizeof( configoverrec ) );
        configOvrFile.Close();
    }
    make_abs_path(full_syscfg.gfilesdir);
    make_abs_path(full_syscfg.datadir);
    make_abs_path(full_syscfg.msgsdir);
    make_abs_path(full_syscfg.dloadsdir);
    make_abs_path(full_syscfg.menudir);

    make_abs_path(syscfgovr.tempdir);
    strncpy( full_syscfg.tempdir, syscfgovr.tempdir, sizeof( full_syscfg.tempdir ) );

    make_abs_path(syscfgovr.batchdir);
    strncpy( full_syscfg.batchdir, syscfgovr.batchdir, sizeof( full_syscfg.batchdir ) );


    // update user info data
    int userreclen = sizeof(userrec);
    int waitingoffset = OFFOF(waiting);
    int inactoffset = OFFOF(inact);
    int sysstatusoffset = OFFOF(sysstatus);
    int fuoffset = OFFOF(forwardusr);
    int fsoffset = OFFOF(forwardsys);
    int fnoffset = OFFOF(net_num);

    if ( userreclen != full_syscfg.userreclen           ||
         waitingoffset != full_syscfg.waitingoffset     ||
         inactoffset != full_syscfg.inactoffset         ||
         sysstatusoffset != full_syscfg.sysstatusoffset ||
         fuoffset != full_syscfg.fuoffset               ||
         fsoffset != full_syscfg.fsoffset               ||
         fnoffset != full_syscfg.fnoffset )
    {
        full_syscfg.userreclen      = userreclen;
        full_syscfg.waitingoffset   = waitingoffset;
        full_syscfg.inactoffset     = inactoffset;
        full_syscfg.sysstatusoffset = sysstatusoffset;
        full_syscfg.fuoffset        = fuoffset;
        full_syscfg.fsoffset        = fsoffset;
        full_syscfg.fnoffset        = fnoffset;
    }

    WFile configFile2( CONFIG_DAT );
    if ( configFile2.Open( WFile::modeReadWrite | WFile::modeBinary ) )
    {
        configFile2.Write( &full_syscfg, sizeof( configrec ) );
        configFile2.Close();
    }
    WFile ovrFile2( CONFIG_OVR );
    if ( !ovrFile2.Open( WFile::modeBinary | WFile::modeReadWrite ) ||
          ovrFile2.GetLength() < static_cast<long>( GetInstanceNumber() * sizeof( configoverrec ) ) )
    {
        ovrFile2.Close();
    }
    else
    {
        ovrFile2.Seek( ( GetInstanceNumber() - 1 ) * sizeof( configoverrec ), WFile::seekBegin );
        ovrFile2.Write( &syscfgovr, sizeof( configoverrec ) );
        ovrFile2.Close();
    }

    syscfg.newuserpw        = WWIV_STRDUP(full_syscfg.newuserpw);
    syscfg.systempw         = WWIV_STRDUP(full_syscfg.systempw);

    syscfg.msgsdir          = WWIV_STRDUP(full_syscfg.msgsdir);
    syscfg.gfilesdir        = WWIV_STRDUP(full_syscfg.gfilesdir);
    syscfg.datadir          = WWIV_STRDUP(full_syscfg.datadir);
    syscfg.dloadsdir        = WWIV_STRDUP(full_syscfg.dloadsdir);
    syscfg.batchdir         = WWIV_STRDUP(full_syscfg.batchdir);
    syscfg.menudir          = WWIV_STRDUP(full_syscfg.menudir);
    syscfg.terminal         = WWIV_STRDUP(full_syscfg.terminal);

    syscfg.systemname       = WWIV_STRDUP(full_syscfg.systemname);
    syscfg.systemphone      = WWIV_STRDUP(full_syscfg.systemphone);
    syscfg.sysopname        = WWIV_STRDUP(full_syscfg.sysopname);
    syscfg.executestr       = WWIV_STRDUP(full_syscfg.executestr);

    syscfg.beginday_c       = WWIV_STRDUP(full_syscfg.beginday_c);
    syscfg.logon_c          = WWIV_STRDUP(full_syscfg.logon_c);
    syscfg.logoff_c         = WWIV_STRDUP(full_syscfg.logoff_c);
    syscfg.newuser_c        = WWIV_STRDUP(full_syscfg.newuser_c);
    syscfg.upload_c         = WWIV_STRDUP(full_syscfg.upload_c);
    syscfg.v_scan_c         = WWIV_STRDUP(full_syscfg.v_scan_c);
    syscfg.dszbatchdl       = WWIV_STRDUP(full_syscfg.dszbatchdl);
    syscfg.dial_prefix      = WWIV_STRDUP(full_syscfg.dial_prefix);

    syscfg.newusersl        = full_syscfg.newusersl;
    syscfg.newuserdsl       = full_syscfg.newuserdsl;
    syscfg.maxwaiting       = full_syscfg.maxwaiting;
    syscfg.newuploads       = full_syscfg.newuploads;
    syscfg.closedsystem     = full_syscfg.closedsystem;

    syscfg.systemnumber     = full_syscfg.systemnumber;
    syscfg.maxusers         = full_syscfg.maxusers;
    syscfg.newuser_restrict = full_syscfg.newuser_restrict;
    syscfg.sysconfig        = full_syscfg.sysconfig;
    syscfg.sysoplowtime     = full_syscfg.sysoplowtime;
    syscfg.sysophightime    = full_syscfg.sysophightime;
    syscfg.executetime      = full_syscfg.executetime;
    syscfg.netlowtime       = full_syscfg.netlowtime;
    syscfg.nethightime      = full_syscfg.nethightime;
    syscfg.max_subs         = full_syscfg.max_subs;
    syscfg.max_dirs         = full_syscfg.max_dirs;
    syscfg.qscn_len         = full_syscfg.qscn_len;
    syscfg.userreclen       = full_syscfg.userreclen;

    syscfg.post_call_ratio  = full_syscfg.post_call_ratio;
    syscfg.req_ratio        = full_syscfg.req_ratio;
    syscfg.newusergold      = full_syscfg.newusergold;

    syscfg.autoval[0]       = full_syscfg.autoval[0];
    syscfg.autoval[1]       = full_syscfg.autoval[1];
    syscfg.autoval[2]       = full_syscfg.autoval[2];
    syscfg.autoval[3]       = full_syscfg.autoval[3];
    syscfg.autoval[4]       = full_syscfg.autoval[4];
    syscfg.autoval[5]       = full_syscfg.autoval[5];
    syscfg.autoval[6]       = full_syscfg.autoval[6];
    syscfg.autoval[7]       = full_syscfg.autoval[7];
    syscfg.autoval[8]       = full_syscfg.autoval[8];
    syscfg.autoval[9]       = full_syscfg.autoval[9];

    syscfg.wwiv_reg_number  = full_syscfg.wwiv_reg_number;
    syscfg.regcode          = full_syscfg.regcode;
    syscfg.sysconfig1       = full_syscfg.sysconfig1;
    syscfg.rrd              = full_syscfg.rrd;

    return true;
}


bool WApplication::SaveConfig()
{
    WFile configFile( CONFIG_DAT );
    if ( configFile.Open( WFile::modeBinary | WFile::modeReadWrite ) )
    {
        configrec full_syscfg;
        configFile.Read( &full_syscfg, sizeof( configrec ) );
        strcpy(full_syscfg.newuserpw, syscfg.newuserpw);
        strcpy(full_syscfg.systempw, syscfg.systempw);

        strcpy(full_syscfg.msgsdir, syscfg.msgsdir);
        strcpy(full_syscfg.gfilesdir, syscfg.gfilesdir);
        strcpy(full_syscfg.datadir, syscfg.datadir);
        strcpy(full_syscfg.dloadsdir, syscfg.dloadsdir);
        strcpy(full_syscfg.batchdir, syscfg.batchdir);
        strcpy(full_syscfg.menudir, syscfg.menudir);
        strcpy(full_syscfg.terminal, syscfg.terminal);

        strcpy(full_syscfg.systemname, syscfg.systemname);
        strcpy(full_syscfg.systemphone, syscfg.systemphone);
        strcpy(full_syscfg.sysopname, syscfg.sysopname);
        strcpy(full_syscfg.executestr, syscfg.executestr);

        strcpy(full_syscfg.beginday_c, syscfg.beginday_c);
        strcpy(full_syscfg.logon_c, syscfg.logon_c);
        strcpy(full_syscfg.logoff_c, syscfg.logoff_c);
        strcpy(full_syscfg.newuser_c, syscfg.newuser_c);
        strcpy(full_syscfg.upload_c, syscfg.upload_c);
        strcpy(full_syscfg.v_scan_c, syscfg.v_scan_c);
        strcpy(full_syscfg.dszbatchdl, syscfg.dszbatchdl);
        strcpy(full_syscfg.dial_prefix, syscfg.dial_prefix);

        full_syscfg.newusersl       = syscfg.newusersl;
        full_syscfg.newuserdsl      = syscfg.newuserdsl;
        full_syscfg.maxwaiting      = syscfg.maxwaiting;
        full_syscfg.newuploads      = syscfg.newuploads;
        full_syscfg.closedsystem    = syscfg.closedsystem;

        full_syscfg.systemnumber    = syscfg.systemnumber;
        full_syscfg.maxusers        = syscfg.maxusers;
        full_syscfg.newuser_restrict = syscfg.newuser_restrict;
        full_syscfg.sysconfig       = syscfg.sysconfig;
        full_syscfg.sysoplowtime    = syscfg.sysoplowtime;
        full_syscfg.sysophightime   = syscfg.sysophightime;
        full_syscfg.executetime     = syscfg.executetime;
        full_syscfg.netlowtime      = syscfg.netlowtime;
        full_syscfg.nethightime     = syscfg.nethightime;
        full_syscfg.max_subs        = syscfg.max_subs;
        full_syscfg.max_dirs        = syscfg.max_dirs;
        full_syscfg.qscn_len        = syscfg.qscn_len;
        full_syscfg.userreclen      = syscfg.userreclen;

        full_syscfg.post_call_ratio = syscfg.post_call_ratio;
        full_syscfg.req_ratio       = syscfg.req_ratio;
        full_syscfg.newusergold     = syscfg.newusergold;

        full_syscfg.autoval[0]      = syscfg.autoval[0];
        full_syscfg.autoval[1]      = syscfg.autoval[1];
        full_syscfg.autoval[2]      = syscfg.autoval[2];
        full_syscfg.autoval[3]      = syscfg.autoval[3];
        full_syscfg.autoval[4]      = syscfg.autoval[4];
        full_syscfg.autoval[5]      = syscfg.autoval[5];
        full_syscfg.autoval[6]      = syscfg.autoval[6];
        full_syscfg.autoval[7]      = syscfg.autoval[7];
        full_syscfg.autoval[8]      = syscfg.autoval[8];
        full_syscfg.autoval[9]      = syscfg.autoval[9];

        for ( int nTempArcNum = 0; nTempArcNum < 4; nTempArcNum++ )
        {
            strcpy( full_syscfg.arcs[ nTempArcNum ].extension, arcs[ nTempArcNum ].extension );
            strcpy( full_syscfg.arcs[ nTempArcNum ].arca, arcs[ nTempArcNum ].arca );
            strcpy( full_syscfg.arcs[ nTempArcNum ].arce, arcs[ nTempArcNum ].arce );
            strcpy( full_syscfg.arcs[ nTempArcNum ].arcl, arcs[ nTempArcNum ].arcl );
        }

        full_syscfg.wwiv_reg_number = syscfg.wwiv_reg_number;
        full_syscfg.sysconfig1      = syscfg.sysconfig1;
        full_syscfg.rrd             = syscfg.rrd;

        configFile.Seek( 0, WFile::seekBegin );
        configFile.Write( &full_syscfg, sizeof( configrec ) );
        return false;
    }
    return true;
}


void WApplication::read_nextern()
{
    GetSession()->SetNumberOfExternalProtocols( 0 );
    if ( externs )
    {
        BbsFreeMemory(externs);
        externs = NULL;
    }

    WFile externalFile( syscfg.datadir, NEXTERN_DAT );
    if ( externalFile.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        unsigned long lFileSize = externalFile.GetLength();
        if ( lFileSize > 15 * sizeof( newexternalrec ) )
        {
            lFileSize = 15 * sizeof( newexternalrec );
        }
        externs = static_cast<newexternalrec *>( BbsAllocWithComment(lFileSize + 10, "external protocols") );
		WWIV_ASSERT( externs != NULL );
        GetSession()->SetNumberOfExternalProtocols( externalFile.Read( externs, lFileSize ) / sizeof( newexternalrec ) );
    }
}


void WApplication::read_arcs()
{
    if ( arcs )
    {
        BbsFreeMemory( arcs );
        arcs = NULL;
    }

    WFile archiverFile( syscfg.datadir, ARCHIVER_DAT );
    if ( archiverFile.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        unsigned long lFileSize = archiverFile.GetLength();
        if ( lFileSize > MAX_ARCS * sizeof( arcrec ) )
        {
            lFileSize = MAX_ARCS * sizeof( arcrec );
        }
        arcs = static_cast<arcrec *>( BbsAllocWithComment( lFileSize, "archivers" ) );
		WWIV_ASSERT( arcs != NULL );
    }
}


void WApplication::read_editors()
{
    if ( editors )
    {
        BbsFreeMemory( editors );
        editors = NULL;
    }
    GetSession()->SetNumberOfEditors( 0 );

    WFile file( syscfg.datadir, EDITORS_DAT );
    if ( file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        unsigned long lFileSize = file.GetLength();
        if ( lFileSize > 10 * sizeof( editorrec ) )
        {
            lFileSize = 10 * sizeof( editorrec );
        }
        editors = static_cast<editorrec *>( BbsAllocWithComment(lFileSize + 10, "external editors") );
		WWIV_ASSERT( editors != NULL );
        GetSession()->SetNumberOfEditors( file.Read( editors, lFileSize ) / sizeof( editorrec ) );
    }
}


void WApplication::read_nintern()
{
    if ( over_intern )
    {
        BbsFreeMemory( over_intern );
        over_intern = NULL;
    }
    WFile file( syscfg.datadir, NINTERN_DAT );
    if ( file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        over_intern = static_cast<newexternalrec *>( BbsAllocWithComment( 3 * sizeof( newexternalrec ), "internal protocol overrides" ) );
		WWIV_ASSERT( over_intern != NULL );

        file.Read( over_intern, 3 * sizeof( newexternalrec ) );
    }
}


bool WApplication::read_subs()
{
    if ( subboards )
    {
        BbsFreeMemory( subboards );
    }
    subboards = NULL;
    GetSession()->SetMaxNumberMessageAreas( syscfg.max_subs );
    subboards = static_cast< subboardrec * >( BbsAllocWithComment( GetSession()->GetMaxNumberMessageAreas() * sizeof( subboardrec ), "subboards" ) );
	WWIV_ASSERT( subboards != NULL );

    WFile file( syscfg.datadir, SUBS_DAT );
    if ( !file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        std::cout << file.GetName() << " NOT FOUND.\r\n";
        return false;
    }
    GetSession()->num_subs = ( file.Read( subboards, ( GetSession()->GetMaxNumberMessageAreas() * sizeof( subboardrec ) ) ) ) / sizeof( subboardrec );
    return ( read_subs_xtr( GetSession()->GetMaxNumberMessageAreas(), GetSession()->num_subs, subboards ) );
}


void WApplication::read_networks()
{
    GetSession()->internetEmailName = "";
    GetSession()->internetEmailDomain = "";
    GetSession()->internetPopDomain = "";
    GetSession()->SetInternetUseRealNames( false );

    WTextFile fileNetIni( "NET.INI", "rt" );
    if ( fileNetIni.IsOpen() )
    {
        while ( !fileNetIni.IsEndOfFile() )
        {
            char szBuffer[ 255 ];
            fileNetIni.ReadLine(szBuffer, 80);
            szBuffer[strlen(szBuffer) - 1] = 0;
            StringRemoveWhitespace(szBuffer);
            if ( !WWIV_STRNICMP( szBuffer, "DOMAIN=", 7 ) && GetSession()->internetEmailDomain.empty() )
            {
                GetSession()->internetEmailDomain = &(szBuffer[7]);
            }
            else if ( !WWIV_STRNICMP(szBuffer, "POPNAME=", 8 ) && GetSession()->internetEmailName.empty() )
            {
                GetSession()->internetEmailName = &( szBuffer[8] );
            }
            else if ( !WWIV_STRNICMP(szBuffer, "FWDDOM=", 7) )
            {
                GetSession()->internetEmailDomain = &(szBuffer[7]);
            }
            else if ( !WWIV_STRNICMP(szBuffer, "FWDNAME=", 8) )
            {
                GetSession()->internetEmailName = &(szBuffer[8]);
            }
            else if ( !WWIV_STRNICMP(szBuffer, "POPDOMAIN=", 10) )
            {
                GetSession()->internetPopDomain = &( szBuffer[10] );
            }
            else if ( !WWIV_STRNICMP(szBuffer, "REALNAME=", 9 ) &&
                      ( szBuffer[9] == 'Y' || szBuffer[9] == 'y' ) )
            {
                GetSession()->SetInternetUseRealNames( true );
            }
        }
        fileNetIni.Close();
    }
    if ( net_networks )
    {
        BbsFreeMemory( net_networks );
    }
    net_networks = NULL;
    WFile file( syscfg.datadir, NETWORKS_DAT );
    if ( file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        GetSession()->SetMaxNetworkNumber( file.GetLength() / sizeof( net_networks_rec ) );
        if ( GetSession()->GetMaxNetworkNumber() )
        {
            net_networks = static_cast<net_networks_rec *>( BbsAllocWithComment( GetSession()->GetMaxNetworkNumber() * sizeof( net_networks_rec ), "networks.dat" ) );
			WWIV_ASSERT(net_networks != NULL);

            file.Read( net_networks, GetSession()->GetMaxNetworkNumber() * sizeof( net_networks_rec ) );
        }
        file.Close();
        for ( int nTempNetNumber = 0; nTempNetNumber < GetSession()->GetMaxNetworkNumber(); nTempNetNumber++ )
        {
            char* ss = strchr( net_networks[nTempNetNumber].name, ' ' );
            if ( ss )
            {
                *ss = '\0';
            }
        }
    }
    if ( !net_networks )
    {
        net_networks = static_cast<net_networks_rec *>( BbsAllocWithComment( sizeof( net_networks_rec ), "networks.dat" ) );
		WWIV_ASSERT( net_networks != NULL );
        GetSession()->SetMaxNetworkNumber( 1 );
        strcpy( net_networks->name, "WWIVnet" );
        strcpy( net_networks->dir, syscfg.datadir );
        net_networks->sysnum = syscfg.systemnumber;
    }
}


bool WApplication::read_names()
{
    if ( smallist )
    {
        BbsFreeMemory( smallist );
    }
    smallist = static_cast<smalrec *>( BbsAllocWithComment( static_cast<long>( syscfg.maxusers ) * static_cast<long>( sizeof( smalrec ) ),
        "names.lst - try decreasing max users in INIT" ) );
	WWIV_ASSERT( smallist != NULL );

    WFile file( syscfg.datadir, NAMES_LST );
    if ( !file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        std::cout << file.GetName() << " NOT FOUND.\r\n";
        return false;
    }
    file.Read( smallist, sizeof( smalrec ) * statusMgr->GetUserCount() );
    file.Close();
    return true;
}


void WApplication::read_voting()
{
    for ( int nTempQuestionNumber = 0; nTempQuestionNumber < 20; nTempQuestionNumber++ )
    {
        questused[ nTempQuestionNumber ] = 0;
    }

    WFile file( syscfg.datadir, VOTING_DAT );
    if ( file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        int n = static_cast<int>( file.GetLength() / sizeof( votingrec ) ) - 1;
        for ( int nTempQuestUsedNum = 0; nTempQuestUsedNum < n; nTempQuestUsedNum++ )
        {
            votingrec v;
            file.Seek( static_cast<long>( nTempQuestUsedNum ) * sizeof(votingrec), WFile::seekBegin );
            file.Read( &v, sizeof( votingrec ) );
            if (v.numanswers)
            {
                questused[ nTempQuestUsedNum ] = 1;
            }
        }
    }
}


bool WApplication::read_dirs()
{
    if ( directories )
    {
        BbsFreeMemory( directories );
    }
    directories = NULL;
    GetSession()->SetMaxNumberFileAreas( syscfg.max_dirs );
    directories = static_cast<directoryrec *>( BbsAllocWithComment(static_cast<long>(GetSession()->GetMaxNumberFileAreas()) * static_cast<long>( sizeof( directoryrec ) ), "directories" ) );
	WWIV_ASSERT(directories != NULL);

    WFile file( syscfg.datadir, DIRS_DAT );
    if ( !file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        std::cout << file.GetName() << "%s NOT FOUND.\r\n";
        return false;
    }
    GetSession()->num_dirs = file.Read( directories, (sizeof(directoryrec) * GetSession()->GetMaxNumberFileAreas()) ) / sizeof(directoryrec);
    return true;
}


void WApplication::read_chains()
{
    if ( chains )
    {
        BbsFreeMemory( chains );
    }
    chains = NULL;
    chains = static_cast<chainfilerec *>( BbsAllocWithComment( GetSession()->max_chains * sizeof( chainfilerec ), "chains" ) );
	WWIV_ASSERT( chains != NULL );
    WFile file( syscfg.datadir, CHAINS_DAT );
    if ( file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        GetSession()->SetNumberOfChains( file.Read( chains, GetSession()->max_chains * sizeof( chainfilerec ) ) / sizeof( chainfilerec ) );
    }
    file.Close();
    if ( GetApplication()->HasConfigFlag( OP_FLAGS_CHAIN_REG ) )
    {
        if ( chains_reg )
        {
            BbsFreeMemory( chains_reg );
        }
        chains_reg = NULL;
        chains_reg = static_cast<chainregrec *>( BbsAllocWithComment( GetSession()->max_chains * sizeof( chainregrec ),
            "chain registration" ) );
		WWIV_ASSERT( chains_reg != NULL );

        WFile regFile( syscfg.datadir, CHAINS_REG );
        if ( regFile.Open( WFile::modeBinary | WFile::modeReadOnly ) )
        {
            regFile.Read( chains_reg, GetSession()->max_chains * sizeof( chainregrec ) );
        }
        else
        {
            for ( int nTempChainNum = 0; nTempChainNum < GetSession()->GetNumberOfChains(); nTempChainNum++ )
            {
                for ( size_t nTempRegByNum = 0; nTempRegByNum < sizeof( chains_reg[ nTempChainNum ].regby ) / sizeof( chains_reg[ nTempChainNum ].regby[0] ); nTempRegByNum++ )
                {
                    chains_reg[ nTempChainNum ].regby[ nTempRegByNum ] = 0;
                }
                chains_reg[ nTempChainNum ].usage   = 0;
                chains_reg[ nTempChainNum ].minage  = 0;
                chains_reg[ nTempChainNum ].maxage  = 255;
            }
            regFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite );
            regFile.Write( chains_reg , sizeof( chainregrec ) * GetSession()->GetNumberOfChains() );
        }
        regFile.Close();
    }
}


bool WApplication::read_language()
{
    if ( languages )
    {
        BbsFreeMemory( languages );
    }
    languages = NULL;
    WFile file( syscfg.datadir, LANGUAGE_DAT );
    if ( file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        GetSession()->num_languages = file.GetLength() / sizeof(languagerec);
        if ( GetSession()->num_languages )
        {
            languages = static_cast<languagerec *>( BbsAllocWithComment(GetSession()->num_languages * sizeof(languagerec), "language.dat") );
			WWIV_ASSERT(languages != NULL);

            file.Read( languages, GetSession()->num_languages * sizeof( languagerec ) );
        }
        file.Close();
    }
    if ( !GetSession()->num_languages )
    {
        languages = static_cast<languagerec *>( BbsAllocWithComment( sizeof( languagerec ), "language.dat" ) );
		WWIV_ASSERT( languages != NULL );
        GetSession()->num_languages = 1;
        strcpy( languages->name, "English" );
        strncpy( languages->dir, syscfg.gfilesdir, sizeof( languages->dir ) - 1 );
        strncpy( languages->mdir, syscfg.menudir, sizeof( languages->mdir ) - 1 );
    }
    GetSession()->SetCurrentLanguageNumber( -1 );
    if ( !set_language( 0 ) )
    {
        std::cout << "You need the default language installed to run the BBS.\r\n";
        return false;
    }
    return true;
}


bool WApplication::read_modem()
{
    if (modem_i)
    {
        BbsFreeMemory(modem_i);
    }
    modem_i = NULL;
    if (!ok_modem_stuff)
    {
        return true;
    }
    char szFileName[ MAX_PATH ];
    if (GetApplication()->GetInstanceNumber() > 1)
    {
        snprintf( szFileName, sizeof( szFileName ), "%s%s.%3.3d", syscfg.datadir, MODEM_XXX, GetApplication()->GetInstanceNumber() );
    }
    else
    {
        snprintf( szFileName, sizeof( szFileName ), "%s%s", syscfg.datadir, MODEM_DAT );
    }
    WFile file( szFileName );
    if ( file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        long lFileSize = file.GetLength();
        modem_i = static_cast<modem_info *>( BbsAllocWithComment(lFileSize, "modem.dat") );
		WWIV_ASSERT(modem_i != NULL);
        file.Read( modem_i, lFileSize );
        return true;
    }
    else
    {
        std::cout << "\r\nRun INIT.EXE to convert modem data.\r\n\n";
        return false;
    }
}


void WApplication::read_gfile()
{
    if (gfilesec != NULL)
    {
        BbsFreeMemory(gfilesec);
		gfilesec = NULL;
    }
    gfilesec = static_cast<gfiledirrec *>( BbsAllocWithComment(static_cast<long>(GetSession()->max_gfilesec * sizeof(gfiledirrec)), "gfiles") );
	WWIV_ASSERT(gfilesec != NULL);
    WFile file( syscfg.datadir, GFILE_DAT );
    if ( !file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        GetSession()->num_sec = 0;
    }
    else
    {
        GetSession()->num_sec = file.Read( gfilesec, GetSession()->max_gfilesec * sizeof( gfiledirrec ) ) / sizeof(gfiledirrec);
    }
}


/**
 * Makes a path into an absolute path, returns true if original path altered,
 * else returns false
 */
bool WApplication::make_abs_path( char *pszDirectory )
{
    char szOldDirectory[ MAX_PATH ];

#ifdef _UNIX
    if ( strlen( pszDirectory ) < 1 )
#else
    if ( strlen( pszDirectory ) < 3 || pszDirectory[1] != ':' || pszDirectory[2] != WWIV_FILE_SEPERATOR_CHAR )
#endif
    {
        WWIV_GetDir( szOldDirectory, true );
        GetApplication()->CdHome();
        WWIV_ChangeDirTo( pszDirectory );
        WWIV_GetDir( pszDirectory, true );
        WWIV_ChangeDirTo( szOldDirectory );
        return true;
    }
    return false;
}


void WApplication::InitializeBBS()
{
    std::string newprompt;

    GetSession()->screenbottom = defscreenbottom = GetLocalIO()->GetDefaultScreenBottom();

    GetLocalIO()->LocalCls();
#if !defined( _UNIX )
    std::cout << std::endl << wwiv_version << beta_version << ", Copyright (c) 1998-2005, WWIV Software Services.\r\n\n";
	std::cout << "\r\nInitializing BBS...\r\n";
#endif // _UNIX
    GetSession()->SetCurrentReadMessageArea( -1 );
    use_workspace = false;
    chat_file = false;
    GetLocalIO()->SetSysopAlert( false );
    nsp = 0;
    GetLocalIO()->set_global_handle( false, true );
    bquote = 0;
    equote = 0;
    GetSession()->SetQuoting( false );
    GetSession()->tagptr = 0;

    time_t t = time( NULL );
    // Struct tm_year is -= 1900
    struct tm * pTm = localtime( &t );
    if ( pTm->tm_year < 88 )
    {
        std::cout << "You need to set the date [" << pTm->tm_year << "] & time before running the BBS.\r\n";
        AbortBBS();
    }
    if (!ReadConfig())
    {
        AbortBBS( true );
    }

    if ( syscfgovr.tempdir[0] == '\0' || !WFile::Exists( syscfgovr.tempdir ) )
    {
        std::cout << "\r\nYour temp dir isn't valid.\r\n";
        std::cout << "It is now set to: '" << syscfgovr.tempdir << "'\r\n\n";
        AbortBBS();
    }

    if ( syscfgovr.batchdir[0] == '\0' || !WFile::Exists( syscfgovr.batchdir ) )
    {
        std::cout << "\r\nYour batch dir isn't valid.\r\n";
        std::cout << "It is now set to: '" << syscfgovr.batchdir << "'\r\n\n";
        AbortBBS();
    }

    write_inst( INST_LOC_INIT, 0, INST_FLAGS_NONE );

    // make sure it is the new USERREC structure
    XINIT_PRINTF("* Reading user scan pointers.\r\n");
    WFile fileQScan( syscfg.datadir, USER_QSC );
    if (!fileQScan.Exists())
    {
        std::cout << "Could not open file '" << fileQScan.GetFullPathName() << "'\r\n";
        std::cout << "You must go into INIT and convert your userlist before running the BBS.\r\n";
        AbortBBS();
    }

#if !defined( _UNIX )
    if ( !syscfgovr.primaryport )
    {
        ok_modem_stuff = false;
    }
#endif // _UNIX

    languages = NULL;
    if (!read_language())
    {
        AbortBBS();
    }

    XINIT_PRINTF("* Processing configuration file: WWIV.INI.\r\n");
    if (!ReadINIFile())
    {
        std::cout << "Insufficient memory for system info structure.\r\n";
        AbortBBS();
    }

    net_networks = NULL;
    GetSession()->SetNetworkNumber( 0 );
    read_networks();
    set_net_num( 0 );

    XINIT_PRINTF( "* Reading status information.\r\n" );
    WStatus* pStatus = statusMgr->BeginTransaction();
    if ( !pStatus ) 
    {
        std::cout << "Unable to return status.dat.\r\n";
        AbortBBS();
    }

    XINIT_PRINTF("* Reading color information.\r\n");
    WFile fileColor( syscfg.datadir, COLOR_DAT );
    if ( !fileColor.Exists() )
	{
        buildcolorfile();
	}
    get_colordata();

    pStatus->SetWWIVVersion( wwiv_num_version );
    pStatus->EnsureCallerNumberIsValid();
    statusMgr->CommitTransaction( pStatus );

    gat = static_cast<unsigned short *>( BbsAllocWithComment( 2048 * sizeof( short ), "gat" ) );
	WWIV_ASSERT( gat != NULL );

    XINIT_PRINTF( "* Reading Gfiles.\r\n" );
    gfilesec = NULL;
    read_gfile();

    smallist = NULL;

    XINIT_PRINTF( "* Reading user names.\r\n" );
    if ( !read_names() )
    {
        AbortBBS();
    }

    XINIT_PRINTF( "* Reading Message Areas.\r\n" );
    subboards = NULL;
    if ( !read_subs() )
    {
        AbortBBS();
    }

    XINIT_PRINTF( "* Reading File Areas.\r\n" );
    directories = NULL;
    if ( !read_dirs() )
    {
        AbortBBS();
    }

    XINIT_PRINTF( "* Reading Chains.\r\n" );
    GetSession()->SetNumberOfChains( 0 );
    chains = NULL;
    read_chains();

#ifndef _UNIX
    XINIT_PRINTF( "* Reading Modem Configuration.\r\n" );
    modem_i = NULL;
    if ( !read_modem() )
    {
        AbortBBS();
    }
#endif
    XINIT_PRINTF( "* Reading File Transfer Protocols.\r\n" );
    externs = NULL;
    read_nextern();

    over_intern = NULL;
    read_nintern();

    XINIT_PRINTF( "* Reading File Archivers.\r\n" );
    read_arcs();
    SaveConfig();

    XINIT_PRINTF(" * Reading Full Screen Message Editors.\r\n" );
    read_editors();

    XINIT_PRINTF("* Parsing WWIV.INI.\r\n");
    WIniFile iniFile( WWIV_INI );
    if ( iniFile.Open( INI_TAG ) )
    {
        GetSession()->SetMessageThreadingEnabled( iniFile.GetBooleanValue( "THREAD_SUBS" ) );
        GetSession()->SetCarbonCopyEnabled( iniFile.GetBooleanValue( "ALLOW_CC_BCC" ) );
    }
    iniFile.Close();
    if (WWIV_make_path(m_attachmentDirectory.c_str()))
    {
        std::cout << "\r\nYour file attachment directory is invalid.\r\n";
        std::cout << "It is now set to: " << m_attachmentDirectory << "'\r\n\n";
        AbortBBS();
    }
    CdHome();

    check_phonenum();                         // dupphone addition

    // allocate sub cache
    iscan1(-1, false);

    batch = static_cast<batchrec *>( BbsAllocWithComment(GetSession()->max_batch * sizeof(batchrec), "batch list") );
	WWIV_ASSERT(batch != NULL);

    XINIT_PRINTF("* Reading User Information.\r\n");
    GetSession()->ReadCurrentUser( 1, false );
    fwaiting = ( GetSession()->GetCurrentUser()->isUserDeleted() ) ? 0 : GetSession()->GetCurrentUser()->GetNumMailWaiting();

    statusMgr->RefreshStatusCache();

    if (syscfg.sysconfig & sysconfig_no_local)
    {
        GetSession()->topdata = WLocalIO::topdataNone;
    }
    else
    {
        GetSession()->topdata = WLocalIO::topdataUser;
    }
    char *ss = getenv("PROMPT");
    newprompt = "PROMPT=WWIV: ";

    if (ss)
    {
        newprompt += ss;
    }
    else
    {
        newprompt += "$P$G";
    }
    // put in our environment since passing the xenviron wasn't working
    // with sync emulated fossil
    _putenv( newprompt.c_str() );
    snprintf( g_szDSZLogFileName, sizeof( g_szDSZLogFileName ), "%sWWIVDSZ.%3.3u", GetHomeDir(), GetInstanceNumber() );

#if !defined (_UNIX)
    std::string dszLogEnvironmentVariable("DSZLOG=");
    dszLogEnvironmentVariable.append(g_szDSZLogFileName );
    int pk = 0;
    ss = getenv( "DSZLOG" );

    if ( !ss )
    {
        _putenv( dszLogEnvironmentVariable.c_str() );
    }
    if (!pk)
    {
        _putenv( "PKNOFASTCHAR=Y" );
    }

#endif // defined (_UNIX)

    m_wwivVerEnvVar = "BBS=";
    m_wwivVerEnvVar += wwiv_version;
    _putenv( m_wwivVerEnvVar.c_str() );
    _putenv( m_networkNumEnvVar.c_str() );

    XINIT_PRINTF("* Reading Voting Booth Configuration.\r\n");
    read_voting();

    XINIT_PRINTF("* Reading External Events.\r\n");
    init_events();
    last_time = time_event - timer();
    if ( last_time < 0.0 )
    {
        last_time += HOURS_PER_DAY_FLOAT * SECONDS_PER_HOUR_FLOAT;
    }

    XINIT_PRINTF("* Allocating Memory for Message/File Areas.\r\n");
    do_event = 0;
    usub = static_cast<usersubrec *>( BbsAllocWithComment( GetSession()->GetMaxNumberMessageAreas() * sizeof( usersubrec ), "usub" ) );
	WWIV_ASSERT(usub != NULL);
    GetSession()->m_SubDateCache = static_cast<UINT32 *>( BbsAllocWithComment(GetSession()->GetMaxNumberMessageAreas() * sizeof(long), "GetSession()->m_SubDateCache") );
	WWIV_ASSERT(GetSession()->m_SubDateCache != NULL);

    udir = static_cast<usersubrec *>( BbsAllocWithComment(GetSession()->GetMaxNumberFileAreas() * sizeof(usersubrec), "udir") );
	WWIV_ASSERT(udir != NULL);
    GetSession()->m_DirectoryDateCache = static_cast<UINT32 *>( BbsAllocWithComment(GetSession()->GetMaxNumberFileAreas() * sizeof(long), "GetSession()->m_DirectoryDateCache") );
	WWIV_ASSERT(GetSession()->m_DirectoryDateCache != NULL);

    uconfsub = static_cast<userconfrec *>( BbsAllocWithComment(MAX_CONFERENCES * sizeof(userconfrec), "uconfsub") );
	WWIV_ASSERT(uconfsub != NULL);
    uconfdir = static_cast<userconfrec *>( BbsAllocWithComment(MAX_CONFERENCES * sizeof(userconfrec), "uconfdir") );
	WWIV_ASSERT(uconfdir != NULL);

    qsc = static_cast<unsigned long *>( BbsAllocWithComment(syscfg.qscn_len, "quickscan") );
	WWIV_ASSERT(qsc != NULL);
    qsc_n = qsc + 1;
    qsc_q = qsc_n + (GetSession()->GetMaxNumberFileAreas() + 31) / 32;
    qsc_p = qsc_q + (GetSession()->GetMaxNumberMessageAreas() + 31) / 32;

    ss = getenv("WWIV_INSTANCE");
    strcpy( m_szNetworkExtension, ".NET" );
    if ( ss )
    {
        int nTempInstanceNumber = atoi( ss );
        if ( nTempInstanceNumber > 0 )
        {
            snprintf( m_szNetworkExtension, sizeof( m_szNetworkExtension ), ".%3.3d", nTempInstanceNumber );
            // Fix... Set the global instance variable to match this.  When you run WWIV with the -n<instance> parameter
            // it sets the WWIV_INSTANCE environment variable, however it wasn't doing the reverse.
            m_nInstance = nTempInstanceNumber;
        }
    }

    read_bbs_list_index();
    frequent_init();
    if ( !m_bUserAlreadyOn )
    {
        tmp_disable_pause( true );
        remove_from_temp( "*.*", syscfgovr.tempdir, true );
        remove_from_temp( "*.*", syscfgovr.batchdir, true );
        tmp_disable_pause( false );
        imodem( true );
        cleanup_net();
    }
    find_devices();

    subconfnum = dirconfnum = 0;

	XINIT_PRINTF( "* Reading Conferences.\r\n" );
    read_all_conferences();

    if ( !m_bUserAlreadyOn )
    {
		sysoplog( "", false );
        sysoplogf( "WWIV %s, inst %ld, brought up at %s on %s.", wwiv_version, GetInstanceNumber(), times(), fulldate() );
		sysoplog( "", false );
    }
    if ( GetInstanceNumber() > 1 )
	{
        char szFileName[MAX_PATH];
        snprintf( szFileName, sizeof( szFileName ), "%s.%3.3u", WWIV_NET_NOEXT, GetInstanceNumber() );
        WFile::Remove(szFileName);
	}
	else
	{
        WFile::Remove( WWIV_NET_DAT );
	}

    srand( time( NULL ) );
    catsl();

    XINIT_PRINTF( "* Saving Instance information.\r\n" );
    write_inst( INST_LOC_WFC, 0, INST_FLAGS_NONE );
}



// begin dupphone additions

void WApplication::check_phonenum()
{
    WFile phoneFile( syscfg.datadir, PHONENUM_DAT );
    if ( !phoneFile.Exists() )
    {
        create_phone_file();
    }
}


void WApplication::create_phone_file()
{
    phonerec p;

    WFile file( syscfg.datadir, USER_LST );
    if ( !file.Open( WFile::modeReadOnly | WFile::modeBinary ) )
    {
        return;
    }
    long lFileSize = file.GetLength();
    file.Close();
    int num = static_cast<int>( lFileSize / sizeof( userrec ) );

    WFile phoneNumFile( syscfg.datadir, PHONENUM_DAT );
    if ( !phoneNumFile.Open( WFile::modeReadWrite | WFile::modeAppend | WFile::modeBinary | WFile::modeCreateFile,
        WFile::shareUnknown, WFile::permReadWrite ) )
    {
        return;
    }

    for ( int nTempUserNumber = 1; nTempUserNumber <= num; nTempUserNumber++ )
    {
        WUser user;
        GetApplication()->GetUserManager()->ReadUser( &user, nTempUserNumber );
        if ( !user.isUserDeleted() )
        {
            p.usernum = nTempUserNumber;
            char szTempVoiceNumber[ 255 ], szTempDataNumber[ 255 ];
            strcpy( szTempVoiceNumber, user.GetVoicePhoneNumber() );
            strcpy( szTempDataNumber, user.GetDataPhoneNumber() );
            if ( szTempVoiceNumber[0] && !strstr( szTempVoiceNumber, "000-" ) )
            {
                strcpy( reinterpret_cast<char*>( p.phone ), szTempVoiceNumber );
                phoneNumFile.Write( &p, sizeof( phonerec ) );
            }
            if ( szTempDataNumber[0] &&
                 !wwiv::stringUtils::IsEquals( szTempVoiceNumber, szTempDataNumber ) &&
                 !strstr( szTempVoiceNumber, "000-" ) )
            {
                strcpy( reinterpret_cast<char*>( p.phone ), szTempDataNumber );
                phoneNumFile.Write( &p, sizeof( phonerec ) );
            }
        }
    }
    phoneNumFile.Close();
}


unsigned long GetFlagsFromIniFile(WIniFile *pIniFile, ini_flags_type * fs, int num, unsigned long flags)
{
    for (int i = 0; i < num; i++)
    {
        const char* ss = INI_OPTIONS_ARRAY[ fs[i].strnum ];
        if ( ss && pIniFile->GetValue( ss ) )
        {
            if ( pIniFile->GetBooleanValue( ss ) )
            {
                if (fs[i].sense)
                {
                    flags &= ~fs[i].value;
                }
                else
                {
                    flags |= fs[i].value;
                }
            }
            else
            {
                if (fs[i].sense)
                {
                    flags |= fs[i].value;
                }
                else
                {
                    flags &= ~fs[i].value;
                }
            }
        }
    }

    return flags;
}


#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER

// end dupphone additions
